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

extern int fd;

//template function for all operations for all paths
static void for_each_target_path(node_sc* source_node, const char* suffix, void (*f)(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx), void* ctx)
{
    if (source_node == NULL || f == NULL)
    {
        return;
    }

    const char* safe_suffix = (suffix != NULL) ? suffix : "";

    pthread_mutex_lock(&source_node->targets.mtx);
    node_tr* current = source_node->targets.head;
    while (current != NULL)
    {
        char* dest_path = concat(2, current->target_full, safe_suffix);
        if (dest_path == NULL)
        {
            current = current->next;
            continue;
        }
        cb(dest_path, current, source_node, ctx);
        free(dest_path);
        current = current->next;
    }
    pthread_mutex_unlock(&source_node->targets.mtx);
}

static void create_empty_files(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx)
{
    //mowimy kompilatorowi ze nie uzywamy
    (void)target;
    (void)source_node;
    const char* src_path = (const char*)ctx;
    int write_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (write_fd < 0)
    {
        ERR("fd");
    }
    if (close(write_fd) < 0)
    {
        ERR("close fd");
    }
    if (src_path != NULL)
    {
        copy_permissions_and_attributes(src_path, dest_path);
    }
}

static void copy_files(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx)
{
    const char* source_path = (const char*)ctx;
    if (source_path == NULL || source_node == NULL)
    {
        return;
    }
    copy(source_path, dest_path, source_node->source_full, target->target_full);
}

static void attribs(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx)
{
    (void)target;
    (void)source_node;
    const char* src_path = (const char*)ctx;
    copy_permissions_and_attributes(src_path, dest_path);
}

static void delete_multi(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx)
{
    (void)target;
    (void)source_node;
    (void)ctx;
    del_handling(dest_path);
}

int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw)
{
    if (flag == FTW_D)
    {
        int wd = inotify_add_watch(fd, path, INOTIFY_MASK);
        if (wd == -1)
        {
            ERR("inotify_add_watch");
        }
        char* suffix = get_end_suffix(_source, (char*)path);
        if (suffix == NULL)
        {
            suffix = strdup("");
            if (suffix == NULL)
            {
                ERR("strdup");
            }
        }
        add_wd_node(&wd_list, wd, _source_friendly, _source, path, suffix);
        free(suffix);
    }
    return 0;
}

void initial_backup(char* source, char* source_friendly, char* target)
{
#ifdef DEBUG
    printf("Doing an initial backup of %s to %s...\n", source, target);
#endif
    node_sc* src_node = find_element_by_source(source_friendly);

    _source = source;
    _target = target;

    if (_source == NULL || _target == NULL)
    {
        ERR("initial_backup");
    }
    nftw(source, backup_walk, 1024, FTW_PHYS);

    if (src_node != NULL && src_node->is_inotify_initialized == 0)
    {
        _source = source;
        _source_friendly = source_friendly;
        nftw(source, backup_walk_inotify_init, 1024, FTW_PHYS);
        src_node->is_inotify_initialized = 1;
        _source_friendly = NULL;
    }
    _source = NULL;
    _target = NULL;
#ifdef DEBUG
    printf("Finished\n");
#endif
}

/*

    DELETION HELPERS

*/
int deleter(const char* path, const struct stat* s, int flag, struct FTW* ftw)
{
    if (remove(path) == -1)
    {
        if (errno != ENOENT)
        {
            ERR("remove");
        }
    }
    return 0;
}

void recursive_deleter(char* path)
{
    if (nftw(path, deleter, 1024, FTW_DEPTH | FTW_PHYS) == -1)
    {
        ERR("nftw");
    }
}

/*

    INOTIFY HELPERS

*/
void create_watcher(char* source, char* target) {}

void new_folder_init(node_sc* source_node, char* path) {
    if (source_node == NULL || path == NULL)
    {
        return;
    }

    _source = source_node->source_full;
    _source_friendly = source_node->source_friendly;
    nftw(path, backup_walk_inotify_init, 1024, FTW_PHYS);

    pthread_mutex_lock(&source_node->targets.mtx);
    node_tr* current = source_node->targets.head;
    
    while (current != NULL)
    {
        _target = current->target_full;
        nftw(path, backup_walk, 1024, FTW_PHYS);
        current = current->next;
    }
    pthread_mutex_unlock(&source_node->targets.mtx);

    _source_friendly = NULL;
    _source = NULL;
    _target = NULL;
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
void del_handling(const char* dest_path){
    if (dest_path == NULL)
    {
        return;
    }
    if (remove(dest_path) != 0)
            {
                if (errno == EACCES)
                {
                    printf("No access to the entry %s\n", dest_path);
                    ERR("remove");
                }
                else if (errno == ENOTEMPTY)
                {
                    // recursively delete files and try again
                    recursive_deleter((char*)dest_path);
                    if (remove(dest_path) != 0)
                    {
                        if (errno != ENOENT)
                        {
                            ERR("remove");
                        }
                    }
                }
                else if (errno != ENOENT)
                {
                    ERR("remove");
                }

            }
}
void debug_printer(char* type, int wd, char* source_friendly, char* path, char * event_name, int is_dir){
    #ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw %s on %s (is_dir: %d)\n", wd,
                  source_friendly, path ,type, event_name,
                   is_dir);
    #endif

}
void event_handler(list_wd* wd_list, Ino_List *inotify)
{
    Ino_List inotify_events = *inotify;
    while (inotify_events.size > 0)
    {
        Ino_Node* event = inotify_events.head;

        int is_dir = (event->mask & IN_ISDIR) != 0;

        Node_wd* wd_node = find_element_by_wd(wd_list, event->wd);
        node_sc* source_node = NULL;
        if (wd_node != NULL)
        {
            source_node = find_element_by_source(wd_node->source_friendly);
        }
        const char* suffix = (event->suffix != NULL) ? event->suffix : "";

        if (source_node == NULL)
        {
            remove_inotify_event(inotify);
            continue;
        }

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

            // mapa cookiesow
            // dodajemy delikwenta
            // jezeli nie ma po MOV_TIME s drugiego eventu, usuwamy
        }
        if (event->mask & IN_MOVED_TO)
        {
            debug_printer("IN_MOVED_TO", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
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

        remove_inotify_event(inotify);
    }
}
