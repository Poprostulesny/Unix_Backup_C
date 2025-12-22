// src/inotify_functions.c
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "inotify_functions.h"
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include "generic_file_functions.h"
#include "list_inotify_events.h"
#include "list_move_events.h"
#include "list_sources.h"
#include "list_wd.h"
#include "lists_common.h"
#include "utils.h"

#ifndef DEBUG
// #define DEBUG
#endif

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

static node_tr *_target_node;
#ifndef BUF_SIZE
#define BUF_SIZE 4096
#endif

#ifndef INOTIFY_MASK
#define INOTIFY_MASK \
    (IN_CREATE | IN_CLOSE_WRITE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB)
#endif

/*
Backup walk with initializing inotify for each directory recursivly, it needs
global variables to be set: _source_node, _source, _source_friendly,
_target_node. It does not copy files.
*/
int backup_walk_inotify_init(const char *path, const struct stat *s, int flag, struct FTW *ftw)
{
    if (flag == FTW_D)
    {
        int wd = inotify_add_watch(_target_node->inotify_fd, path, INOTIFY_MASK);
        if (wd == -1)
        {
            ERR("inotify_add_watch");
        }
        char *suffix = get_end_suffix(_source, (char *)path);
        if (suffix == NULL)
        {
            suffix = strdup("");
            if (suffix == NULL)
            {
                ERR("strdup");
            }
        }
        add_wd_node(&_target_node->watchers, wd, _source_friendly, _source, path, suffix);
        free(suffix);
    }
    return 0;
}
/*
Initial backup function with inotify initialization it checks whether inotify
has been instantiated for the given source,and if not it instanitates it. The
function sets the following global variables for use in its revursive calls:
_source_node, _source, _source_friendly, _target_node, _target
*/
void initial_backup(node_sc *source_node, node_tr *target_node)
{
#ifdef DEBUG
    printf("Doing an initial backup of %s to %s...\n", source_node->source_full, target_node->target_full);
#endif

    // Setting variables needed for nftw
    _source = source_node->source_full;
    _target = target_node->target_full;
    _source_friendly = source_node->source_friendly;
    _target_node = target_node;
    if (_source == NULL || _target == NULL)
    {
        ERR("initial_backup");
    }
    // Instantiating inotify for the source if not present
    if (target_node->inotify_fd < 0)
    {
        target_node->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (target_node->inotify_fd == -1)
        {
            ERR("inotify init");
        }
        nftw(source_node->source_full, backup_walk_inotify_init, 1024, FTW_PHYS);
    }

    // Copying files to a given target
    nftw(source_node->source_full, backup_walk, 1024, FTW_PHYS);

    _source = NULL;
    _target = NULL;
    _source_friendly = NULL;
    _target_node = NULL;

#ifdef DEBUG
    printf("Finished\n");
#endif
}

// Recursive initalization of a new folder in an inotify domain.
void new_folder_init(node_sc *source_node, node_tr *target_node, char *path)
{
    if (source_node == NULL || target_node == NULL || path == NULL)
    {
        return;
    }

    // Variables needed for nftw
    _source = source_node->source_full;
    _source_friendly = source_node->source_friendly;
    _target_node = target_node;

    nftw(path, backup_walk_inotify_init, 1024, FTW_PHYS);

    _target = target_node->target_full;
    nftw(path, backup_walk, 1024, FTW_PHYS);

    _source = NULL;
    _source_friendly = NULL;
    _target = NULL;
    _target_node = NULL;
}

// Function to read from the given inotify fd. It adds all events of a given fd
// to the given inotify list.
void inotify_reader(int fd, list_wd *wd_list, Ino_List *inotify)
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
                struct inotify_event *event = (struct inotify_event *)(buffer + offset);

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

                // Calculating the padding (by posix standard each entry aligned to 4
                // bytes)
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
// Helper function used to clean up the code
void debug_printer(char *type, int wd, char *source_friendly, char *path, char *event_name, int is_dir)
{
#ifdef DEBUG
    printf("Watch %d in folder %s (%s) saw %s on %s (is_dir: %d)\n", wd, source_friendly, path, type, event_name,
           is_dir);
#endif
}

// Function handling the Inotify Events of a given source node/target pair.
void event_handler(node_sc *source_node, node_tr *target_node)
{
    if (source_node == NULL || target_node == NULL)
    {
        return;
    }
    // Helper variables
    Ino_List *inotify_events = &target_node->events;
    list_wd *wd_list = &target_node->watchers;

    while (1)
    {
        Ino_Node *event = inotify_events->head;
        // break condition -> no events
        if (event == NULL)
        {
            break;
        }

        int is_dir = (event->mask & IN_ISDIR) != 0;

        // Finding which watch descriptor has encountered the event (at which path
        // it happened)
        Node_wd *wd_node = find_element_by_wd(wd_list, event->wd);
        size_t src_len = strlen(source_node->source_full);
        int outside_source = 1;

        // check whether the given event happened inside the source directory. This
        // check ensures that a moved out, but not yet deleted watcher doesn't
        // trigger a backup to an unspecified location
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
            inotify_rm_watch(target_node->inotify_fd, wd_node->wd);
            delete_wd_node(&target_node->watchers, wd_node->wd);
            remove_inotify_event(inotify_events);
            // no error check since if it didnt exist at this moment than all is good
            continue;
        }

        const char *suffix = (event->suffix != NULL) ? event->suffix : "";
        if ((event->mask & IN_CREATE) && is_dir)
        {
            debug_printer("IN_CREATE", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            new_folder_init(source_node, target_node, event->full_path);
        }
        if ((event->mask & IN_CREATE && !is_dir))
        {
            debug_printer("IN_CREATE", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            char *dest_path = concat(2, target_node->target_full, suffix);
            if (dest_path != NULL)
            {
                create_empty_files(dest_path, target_node, NULL, event->full_path);
                free(dest_path);
            }
        }
        if ((event->mask & IN_CLOSE_WRITE) && !is_dir)
        {
            debug_printer("IN_CLOSE_WRITE", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            char *dest_path = concat(2, target_node->target_full, suffix);
            if (dest_path != NULL)
            {
                copy_files(dest_path, target_node, NULL, (void *)event->full_path);
                free(dest_path);
            }
        }

        if (event->mask & IN_DELETE)
        {
            debug_printer("IN_DELETE", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            char *dest_path = concat(2, target_node->target_full, suffix);
            if (dest_path != NULL)
            {
                delete_multi(dest_path, target_node, NULL, NULL);
                free(dest_path);
            }
        }
        if (event->mask & IN_MOVED_FROM)
        {
            debug_printer("IN_MOVED_FROM", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            add_move_event(&target_node->mov_dict, event->cookie, event->full_path, 1, source_node, target_node);

            // mapa cookiesow
            // dodajemy delikwenta
            // jezeli nie ma po MOV_TIME s drugiego eventu, usuwamy
        }
        if (event->mask & IN_MOVED_TO)
        {
            debug_printer("IN_MOVED_TO", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            add_move_event(&target_node->mov_dict, event->cookie, event->full_path, 0, source_node, target_node);
            // jezeli istnieje in_moved_from z cookiesem matchujacym nasz ->
            // przenosimy jezeli po MOV_TIME s nie ma, kopiujemy plik
        }
        if (event->mask & IN_ATTRIB)
        {
            debug_printer("IN_ATTRIB", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            char *dest_path = concat(2, target_node->target_full, suffix);
            if (dest_path != NULL)
            {
                attribs(dest_path, target_node, source_node, event->full_path);
                free(dest_path);
            }
        }

        if (event->mask & IN_DELETE_SELF)
        {
            debug_printer("IN_DELETE_SELF", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            char *dest_path = concat(2, target_node->target_full, suffix);
            if (dest_path != NULL)
            {
                delete_multi(dest_path, target_node, source_node, NULL);
                free(dest_path);
            }
            /* event->name is empty for IN_DELETE_SELF; compare full paths */
            if (strcmp(event->full_path, source_node->source_full) == 0)
            {
                kill(getpid(), SIGUSR2);
            }
        }
        if (event->mask & IN_MOVE_SELF)
        {
            debug_printer("IN_MOVE_SELF", event->wd, wd_node->source_friendly, wd_node->path, event->name, is_dir);
            if (strcmp(event->full_path, source_node->source_full) == 0)
            {
                kill(getpid(), SIGUSR2);
            }
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
