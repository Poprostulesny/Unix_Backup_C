#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define DEBUG
#include "file_utils.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "lists.h"

/*
    GLOBAL VARIABLES
*/
char* _source;
char* _source_friendly;
char* _target;
int fd;
/*
    HELPER FUNCTIONS
*/
void checked_mkdir(const char* path)
{
    if (mkdir(path, 0755) != 0)
    {
        if (errno != EEXIST)
            ERR("mkdir");
    }
    struct stat s;
    if (stat(path, &s))
        ERR("stat");
    if (!S_ISDIR(s.st_mode))
    {
#ifdef DEBUG
        printf("%s is not a valid dir, exiting\n", path);
#endif

        exit(EXIT_FAILURE);
    }
}

void make_path(char* path)
{
    char* temp = strdup(path);
    if (!temp)
        ERR("strdup");
    for (char* p = temp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            checked_mkdir(temp);
            *p = '/';
        }
    }

    free(temp);
}

/*checking whether the directory exists and is empty. if it doesnt exist it creates it.
    return -1 when is not a directory
    return 0 when it is not empty or didnt exist
*/
int is_empty_dir(char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        make_path(path);
        checked_mkdir(path);
        return 0;
    }

    if (!S_ISDIR(st.st_mode))
    {
        return -1;
    }

    DIR* d = opendir(path);
    if (!d)
    {
        return -1;
    }

    struct dirent* ent;
    int cnt = 0;
    while ((ent = readdir(d)) != NULL)
    {
        cnt++;
    }
    closedir(d);

    if (cnt == 2)
    {
        return 0;
    }

    return -1;
}

int min(int a, int b) { return a < b ? a : b; }

// returns 1 if target is in source, 0 otherwise
int is_target_in_source(char* source, char* target)
{
    int i = 0;
    int n = min((int)strlen(target), (int)strlen(source));
    while (source[i] == target[i] && i < n)
    {
        i++;
    }
    if (i == (int)strlen(source))
    {
        return 1;
    }

    return 0;
}

void copy_permissions_and_attributes(const char* source, const char* dest)
{
    struct stat s;
    if (stat(source, &s) == -1)
    {
        ERR("stat");
    }
    mode_t perms = s.st_mode & 07777;  // we pull sticky, perms, and specials
    struct timespec t[2];
    t[0].tv_nsec = UTIME_OMIT;
    t[0].tv_sec = 0;
    t[1] = s.st_mtim;

    if (chmod(dest, perms) == -1)
    {
        ERR("chmod");
    }
    if (utimensat(AT_FDCWD, dest, t, 0) == -1)
    {
        ERR("utimensat");
    }
}

char* concat(int n, ...)
{
    int total_lenght = 0;
    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++)
    {
        total_lenght += strlen(va_arg(args, char*));
    }
    va_end(args);

    char* re = malloc(total_lenght + 1);
    if (!re)
        ERR("malloc");
    re[0] = 0;

    va_start(args, n);
    for (int i = 0; i < n; i++)
    {
        strcat(re, va_arg(args, char*));
    }
    va_end(args);
    return re;
}
/*-----------------------------------------------------*/

/*

    INITIAL BACKUPS

*/
/*Function that gets the full path for source and target, and current whole path, and returns the path from target to
 * path in target */
char* get_path_to_target(const char* source, const char* target, const char* path)
{
    int s, t, p;
    s = strlen(source);
    t = strlen(target);
    p = strlen(path);
    char* new_path = malloc(sizeof(char) * (p - s + t + 5));
    // if(!new_path) ERR("malloc");
    int i = 0;
    while (i < t)
    {
        new_path[i] = target[i];
        i++;
    }
    int j = s;
    if (new_path[i - 1] == '/' && path[j] == '/')
    {
        j++;
    }
    else if (new_path[i - 1] != '/' && path[j] != '/')
    {
        new_path[i] = '/';
        i++;
    }
    while (j < p)
    {
        new_path[i] = path[j];
        i++;
        j++;
    }
    if (new_path[i - 1] == '/')
    {
        new_path[i - 1] = '\0';
    }
    else
    {
        new_path[i] = '\0';
    }

    return new_path;
}
void copy_file(const char* path1, const char* path2)
{
#ifdef DEBUG
    printf("Copying file %s to %s\n", path1, path2);
#endif

    int read_fd = open(path1, O_RDONLY);
    int write_fd = open(path2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (read_fd < 0)
    {
        ERR("read fd");
    }
    if (write_fd < 0)
    {
        ERR("write fd");
    }
    char buff[1024];
    int r = 0;
    while ((r = read(read_fd, buff, sizeof(buff))) > 0)
    {
        ssize_t offset = 0;
        while (offset < r)
        {
            ssize_t w = write(write_fd, buff + offset, r - offset);
            if (w < 0)
            {
                close(read_fd);
                close(write_fd);
                ERR("copy file");
            }
            offset += w;
        }
    }
    close(read_fd);
    close(write_fd);
    if (r < 0)
    {
        ERR("copy file");
    }
    copy_permissions_and_attributes(path1, path2);
}
void copy_link(const char* path_where, const char* path_dest)
{
#ifdef DEBUG
    printf("Copying symlink %s to %s\n", path_where, path_dest);
#endif

    char buff[1024];
    ssize_t l = readlink(path_where, buff, sizeof(buff) - 1);
    if (l < 0)
    {
        ERR("readlink");
    }
    buff[l] = '\0';
    if (is_target_in_source(_source, buff) == 1)
    {
        char* path_updated = get_path_to_target(_source, _target, buff);
        if (symlink(path_updated, path_dest) < 0)
        {
            ERR("symlink");
        }
        free(path_updated);
    }
    else
    {
        if (symlink(buff, path_dest) < 0)
        {
            ERR("symlink");
        }
    }
}

int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw)
{
    char* path_new = get_path_to_target(_source, _target, path);
    if (path_new == NULL)
    {
        ERR("get_path_to_target");
    }
    if (flag == FTW_D)
    {
        checked_mkdir(path_new);
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
        int wd = inotify_add_watch(fd, path, INOTIFY_MASK);
        if (wd == -1)
        {
            ERR("inotify_add_watch");
        }
        add_wd_node(wd, _source_friendly, _source, path_new, path);
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

    _source = strdup(source);

    _source_friendly = strdup(source_friendly);

    _target = strdup(target);

    if (_source == NULL || _source_friendly == NULL || _target == NULL)
    {
        ERR("strdup");
    }
    nftw(source, backup_walk_inotify_init, 1024, FTW_PHYS);
    free(_source);
    free(_source_friendly);
    free(_target);
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

void new_folder_init(char* source, char* source_friendly, char* path) {}

void inotify_reader()
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
                add_inotify_event(event);
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
void del_handling(Ino_Node * event){
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
void event_handler()
{
    while (inotify_events.size > 0)
    {
        Ino_Node* event = inotify_events.head;

        int is_dir = (event->mask & IN_ISDIR) != 0;

        Node_wd* wd_node = find_element_by_wd(event->wd);

        if ((event->mask & IN_CREATE) && is_dir)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_CREATE on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // tworzymy backup folderu
            
        }
        if ((event->mask & IN_CREATE && !is_dir))
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_CREATE on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif

            int write_fd = open(event->full_path_dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0)
            {
                ERR("fd");
            }
            if (close(write_fd) < 0)
            {
                ERR("close fd");
            }
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
            copy_file(event->full_path, event->full_path_dest);
        }

        if (event->mask & IN_DELETE)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_DELETE on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            
            del_handler(event);
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
            del_handler(event);
        }
        if (event->mask & IN_MOVE_SELF)
        {
#ifdef DEBUG
            printf("Watch %d in folder %s (%s) saw IN_MOVE_SELF on %s (is_dir: %d)\n", event->wd,
                   wd_node ? wd_node->source_friendly : "unknown", wd_node ? wd_node->path : "unknown", event->name,
                   is_dir);
#endif
            // jezeli nie ma move to, to wywalamy na pysk
        }
        if (event->mask & IN_IGNORED)
        {
            delete_wd_node(event->wd);
        }

        remove_inotify_event();
    }
}
