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
#include "inotify_functions.h"
#include "generic_file_functions.h"
#include "lists_common.h"
#include "list_wd.h"
#include "list_inotify_events.h"

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
    char* path_new = get_path_to_target(_source, _target, path);
    if (path_new == NULL)
    {
        ERR("get_path_to_target");
    }
    if (flag == FTW_D)
    {
        checked_mkdir(path_new);
        int wd = inotify_add_watch(_fd, path, INOTIFY_MASK);
        if (wd == -1)
        {
            ERR("inotify_add_watch");
        }
        add_wd_node(&wd_list, wd, _source_friendly, _source, path_new, path, path_new);
    }
    else if (flag == FTW_F)
    {
        copy_file(path, path_new);
    }

    else if (flag == FTW_SL)
    {
        copy_link(path, path_new);
    }
    free(path_new);
    return 0;
}

void initial_backup(char* source, char* source_friendly, char* target)
{
#ifdef DEBUG
    printf("Doing an initial backup of %s to %s...\n", source, target);
#endif

    _source = source;
    _source_friendly = source_friendly;
    _target = target;

    if (_source == NULL || _source_friendly == NULL || _target == NULL)
    {
        ERR("strdup");
    }
    nftw(source, backup_walk_inotify_init, 1024, FTW_PHYS);
    _source_friendly = NULL;
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

void new_folder_init(char* source, char* source_friendly, char* path, char* target, int fd) {
    _fd=fd;
    _source = source;
    _target = target;
    _source_friendly = source_friendly;
    
    nftw(path, backup_walk_inotify_init, 1024, FTW_PHYS);
    _source_friendly = NULL;
    _source = NULL;
    _target = NULL;
    _fd = 0;
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
void del_handling(void* event_ptr){
    Ino_Node* event = (Ino_Node*)event_ptr;
    if (remove(event->full_path_dest) != 0)
            {
                if (errno == EACCES)
                {
                    printf("No access to the entry %s\n", event->full_path_dest);
                    ERR("remove");
                }
                else if (errno == ENOTEMPTY)
                {
                    // recursively delete files and try again
                    recursive_deleter(event->full_path_dest);
                    if (remove(event->full_path_dest) != 0)
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

void event_handler(list_wd* wd_list, Ino_List *inotify)
{
    Ino_List inotify_events = *inotify;
    while (inotify_events.size > 0)
    {
        Ino_Node* event = inotify_events.head;

        int is_dir = (event->mask & IN_ISDIR) != 0;

        Node_wd* wd_node = find_element_by_wd(wd_list, event->wd);

        if ((event->mask & IN_CREATE) && is_dir)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_CREATE on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // tworzymy backup folderu
            new_folder_init(wd_node->source_full, wd_node->source_friendly, event->full_path, wd_node->full_target_path );
            
        }
        if ((event->mask & IN_CREATE && !is_dir))
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_CREATE on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif

            int write_fd = open(event->full_path_dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (write_fd < 0)
            {
                ERR("fd");
            }
            if (close(write_fd) < 0)
            {
                ERR("close fd");
            }
            copy_permissions_and_attributes(event->full_path, event->full_path_dest);
            // tworzymy nowy pusty plik
            // jezeli on już jest - wtedy nie robimy nic
        }
        if ((event->mask & IN_CLOSE_WRITE) && !is_dir)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_CLOSE_WRITE on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // kopiujemy plik
            copy(event->full_path, event->full_path_dest, wd_node->source_full, wd_node->full_target_path);
        }

        if (event->mask & IN_DELETE)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_DELETE on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            
            del_handling(event);
            // usun
        }
        if (event->mask & IN_MOVED_FROM)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_MOVED_FROM on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // mapa cookiesow
            // dodajemy delikwenta
            // jezeli nie ma po 8s drugiego eventu, usuwamy
        }
        if (event->mask & IN_MOVED_TO)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_MOVED_TO on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // jezeli istnieje in_moved_from z cookiesem matchujacym nasz -> przenosimy
            // jezeli po 8s nie ma, kopiujemy plik
        }
        if (event->mask & IN_ATTRIB)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_ATTRIB on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // zmieniamy atrybuty
            copy_permissions_and_attributes(event->full_path, event->full_path_dest);
        }

        if (event->mask & IN_DELETE_SELF)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_DELETE_SELF on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // usuwamy i usuwamy watch
            del_handling(event);
        }
        if (event->mask & IN_MOVE_SELF)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_MOVE_SELF on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // brak specjlnego handlingu move nam to ogarnia
        }
        if (event->mask & IN_IGNORED)
        {
            delete_wd_node(wd_list, event->wd);
        }

        remove_inotify_event(inotify);
    }
}
