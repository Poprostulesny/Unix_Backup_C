// src/inotify_functions.c
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <sys/inotify.h>
#include <sys/stat.h>
#include <ftw.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "inotify_functions.h"
#include "generic_file_functions.h"
#include "lists_common.h"
#include "list_wd.h"
#include "list_inotify_events.h"
#include "list_sources.h"
#include "list_move_events.h"
#include "utils.h"

#ifndef DEBUG
#define DEBUG
#endif

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

#ifndef BUF_SIZE
#define BUF_SIZE 4096
#endif

#ifndef INOTIFY_MASK
#define INOTIFY_MASK (IN_CREATE | IN_CLOSE_WRITE | IN_DELETE | IN_DELETE_SELF | \
                      IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB)
#endif



int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw)
{
    if (flag == FTW_D)
    {
        int wd = inotify_add_watch(_source_node->fd, path, INOTIFY_MASK);
        if (wd == -1)
        {
            ERR("inotify_add_watch");
        }
        backup_ctx_lock();
        char* suffix = get_end_suffix(_source, (char*)path);
        if (suffix == NULL)
        {
            suffix = strdup("");
            if (suffix == NULL)
            {
                ERR("strdup");
            }
        }
        add_wd_node(&_source_node->watchers, wd, _source_friendly, _source, path, suffix);
        backup_ctx_unlock();
        free(suffix);
    }
    return 0;
}

void initial_backup(node_sc* source_node, char* source_friendly, char* target)
{
#ifdef DEBUG
    printf("Doing an initial backup of %s to %s...\n", source_node->source_full, target);
#endif
    node_sc* src_node = find_element_by_source(source_friendly);

    backup_ctx_lock();
    _source = source_node->source_full;
    _target = target;
    _source_node = source_node;
    _source_friendly = source_friendly;
    if (_source == NULL || _target == NULL)
    {
        ERR("initial_backup");
    }
    nftw(source_node->source_full, backup_walk, 1024, FTW_PHYS);

    if (src_node != NULL && src_node->is_inotify_initialized == 0)
    {
        
       src_node->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
       if(src_node->fd==-1){
        ERR("inotify init");
       }
        nftw(source_node->source_full, backup_walk_inotify_init, 1024, FTW_PHYS);
        src_node->is_inotify_initialized = 1;
        
    }
    _source_friendly = NULL;
    _source_node=NULL;
    _source = NULL;
    _target = NULL;
    backup_ctx_unlock();
#ifdef DEBUG
    printf("Finished\n");
#endif
}

void new_folder_init(node_sc* source_node, char* path) {
    if (source_node == NULL || path == NULL)
    {
        return;
    }

    backup_ctx_lock();
    _source = source_node->source_full;
    _source_friendly = source_node->source_friendly;
    _source_node = source_node;
    
    
    nftw(path, backup_walk_inotify_init, 1024, FTW_PHYS);

    // Build a snapshot of targets to avoid holding the lock during nftw
    pthread_mutex_lock(&source_node->targets.mtx);
    int target_count = source_node->targets.size;
    char** target_paths = NULL;
    
    if (target_count > 0) {
        target_paths = malloc(sizeof(char*) * target_count);
        if (target_paths == NULL) {
            pthread_mutex_unlock(&source_node->targets.mtx);
            ERR("malloc");
        }
        
        node_tr* current = source_node->targets.head;
        int i = 0;
        while (current != NULL && i < target_count) {
            target_paths[i] = strdup(current->target_full);
            if (target_paths[i] == NULL) {
                // Clean up already allocated strings
                for (int j = 0; j < i; j++) {
                    free(target_paths[j]);
                }
                free(target_paths);
                pthread_mutex_unlock(&source_node->targets.mtx);
                ERR("strdup");
            }
            current = current->next;
            i++;
        }
    }
    pthread_mutex_unlock(&source_node->targets.mtx);
    
    // Now walk the path for each target WITHOUT holding the mutex
    for (int i = 0; i < target_count; i++) {
        _target = target_paths[i];
        nftw(path, backup_walk, 1024, FTW_PHYS);
    }
    
    // Clean up the snapshot
    for (int i = 0; i < target_count; i++) {
        free(target_paths[i]);
    }
    free(target_paths);
    
    _source_node = NULL;
    _source_friendly = NULL;
    _source = NULL;
    _target = NULL;
    backup_ctx_unlock();
}

void inotify_reader(int fd, list_wd* wd_list, Ino_List *inotify)
{
    
    char buffer[BUF_SIZE];
    ssize_t bytes_read = 0;

#ifdef DEBUG
    printf("[inotify_reader] start\n");
#endif

    while (1)
    {
        bytes_read = read(fd, buffer, BUF_SIZE);

        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
#ifdef DEBUG
                printf("[inotify_reader] no data (EAGAIN/EWOULDBLOCK), exit loop\n");
#endif
                break;
            }
            else
            {
                ERR("read");
                break;
            }
        }
        else if (bytes_read > 0)
        {
            size_t offset = 0;
            // Computing every struct in the buffer
            while (offset < (size_t)bytes_read)
            {
                struct inotify_event* event = (struct inotify_event*)(buffer + offset);

                // Is there enough data to fill the struct
                if (offset + sizeof(struct inotify_event) > (size_t)bytes_read)
                {
                    fprintf(stderr, "Partial header read!\n");
                    break;
                }

                // Is there enough space for the name
                size_t event_size = sizeof(struct inotify_event) + event->len;
                if (offset + event_size > (size_t)bytes_read)
                {
                    ERR("partial read");
                    break;
                }
#ifdef DEBUG
                printf("File: %.*s\n", event->len, event->name);
#endif
                add_inotify_event(wd_list, inotify, event);
                // Calculating the padding (by posix standard each entry aligned to 4 bytes)
                size_t padding = (4 - (event_size % 4)) % 4;
                offset += event_size + padding;
            }
        }

#ifdef DEBUG
        printf("[inotify_reader] end\n");
#endif
    }
}

/*

    INOTIFY EVENT HANDLING

*/
void debug_printer(char* type, int wd, char* source_friendly, char* path, char * event_name, int is_dir){
    #ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw %s on %s (is_dir: %d)\n", wd,
                  source_friendly, path ,type, event_name,
                   is_dir);
    #endif

}
void event_handler(node_sc *source_node)
{
    Ino_List *inotify_events = &source_node->events;
    list_wd *wd_list = &source_node->watchers;
    while (1)
    {
        pthread_mutex_lock(&inotify_events->mtx);
        Ino_Node* event = inotify_events->head;
        if (event == NULL)
        {
            pthread_mutex_unlock(&inotify_events->mtx);
            break;
        }
        pthread_mutex_unlock(&inotify_events->mtx);

        int is_dir = (event->mask & IN_ISDIR) != 0;

        Node_wd* wd_node = find_element_by_wd(wd_list, event->wd);
        size_t src_len = strlen(source_node->source_full);
        int outside_source = 1;
        if (strncmp(event->full_path, source_node->source_full, src_len) == 0)
        {
            char boundary = event->full_path[src_len];
            if (boundary == '\0' || boundary == '/')
            {
                outside_source = 0;
            }
        }
        if (outside_source)
        {   
            inotify_rm_watch(source_node->fd, wd_node->wd);
            delete_wd_node(&source_node->watchers, wd_node->wd);
            remove_inotify_event(inotify_events);
            continue;
        }
        const char* suffix = (event->suffix != NULL) ? event->suffix : "";
        if ((event->mask & IN_CREATE) && is_dir)
        {
            debug_printer("IN_CREATE", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            new_folder_init(source_node, event->full_path);
        }
        if ((event->mask & IN_CREATE && !is_dir))
        {
            debug_printer("IN_CREATE", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            for_each_target_path(source_node, suffix, create_empty_files, event->full_path);
        }
        if ((event->mask & IN_CLOSE_WRITE) && !is_dir)
        {
            debug_printer("IN_CLOSE_WRITE", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            for_each_target_path(source_node, suffix, copy_files, (void*)event->full_path);
        }

        if (event->mask & IN_DELETE)
        {
            debug_printer("IN_DELETE", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            for_each_target_path(source_node, suffix, delete_multi, NULL);
        }
        if (event->mask & IN_MOVED_FROM)
        {
            debug_printer("IN_MOVED_FROM", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            add_move_event(&source_node->mov_dict,event->cookie, event->full_path, 1, source_node);
            // mapa cookiesow
            // dodajemy delikwenta
            // jezeli nie ma po MOV_TIME s drugiego eventu, usuwamy
        }
        if (event->mask & IN_MOVED_TO)
        {
            debug_printer("IN_MOVED_TO", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            add_move_event(&source_node->mov_dict,event->cookie, event->full_path, 0, source_node);
            // jezeli istnieje in_moved_from z cookiesem matchujacym nasz -> przenosimy
            // jezeli po MOV_TIME s nie ma, kopiujemy plik
        }
        if (event->mask & IN_ATTRIB)
        {
            debug_printer("IN_ATTRIB", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            for_each_target_path(source_node, suffix, attribs, event->full_path);
        }

        if (event->mask & IN_DELETE_SELF)
        {
            debug_printer("IN_DELETE_SELF", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            for_each_target_path(source_node, suffix, delete_multi, NULL);
        }
        if (event->mask & IN_MOVE_SELF)
        {
            debug_printer("IN_MOVE_SELF", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            // brak specjlnego handlingu move nam to ogarnia
          

            
        }
        if (event->mask & IN_IGNORED)
        {   
            debug_printer("IN_IGNORED", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            delete_wd_node(wd_list, event->wd);
        }

        remove_inotify_event(inotify_events);
    }
}
