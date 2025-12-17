#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "lists_common.h"
#include "generic_file_functions.h"
#include "utils.h"

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

/* defined here, declared in header */
node_sc* _source_node;
static pthread_mutex_t backup_ctx_mtx;
static pthread_once_t backup_ctx_once = PTHREAD_ONCE_INIT;

static void backup_ctx_init(void)
{
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0)
    {
        ERR("pthread_mutexattr_init");
    }
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
    {
        ERR("pthread_mutexattr_settype");
    }
    if (pthread_mutex_init(&backup_ctx_mtx, &attr) != 0)
    {
        ERR("pthread_mutex_init");
    }
    pthread_mutexattr_destroy(&attr);
}

void backup_ctx_lock(void)
{
    pthread_once(&backup_ctx_once, backup_ctx_init);
    pthread_mutex_lock(&backup_ctx_mtx);
}

void backup_ctx_unlock(void)
{
    pthread_mutex_unlock(&backup_ctx_mtx);
}

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

    /* Ensure destination directory exists */
    char* dest_dir = strdup(path2);
    if (dest_dir == NULL)
    {
        ERR("strdup");
    }
    char* last_sep = strrchr(dest_dir, '/');
    if (last_sep != NULL)
    {
        *last_sep = '\0';
#ifdef DEBUG
        printf("[copy_file] making path for dest dir: %s\n", dest_dir);
#endif
        make_path(dest_dir);
        /* make_path builds all parents, but not the last component; ensure the leaf dir exists */
        checked_mkdir(dest_dir);
    }
#ifdef DEBUG
    else
    {
        printf("[copy_file] no directory component in path: %s\n", path2);
    }
#endif
    free(dest_dir);

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

void copy(const char* source, const char* dest, char* backup_source, char* backup_target) {
    struct stat st;

    if (lstat(source, &st) == -1) {
        if (errno != ENOENT) {
            ERR("lstat");
        } else {
            return;
        }
    }

    backup_ctx_lock();
    if (S_ISLNK(st.st_mode)) {
        _source = backup_source;
        _target = backup_target;
        copy_link(source, dest);
        _target = NULL;
        _source = NULL;
    } else if (S_ISREG(st.st_mode)) {
        copy_file(source, dest);
    }
    backup_ctx_unlock();
}

void copy_to_all_targets(const char* source_path, const char* file_suffix, list_tg* l, char* source_full)
{
    if (source_path == NULL || file_suffix == NULL || l == NULL || source_full == NULL)
    {
        return;
    }

    struct stat st;
    if (lstat(source_path, &st) == -1)
    {
        if (errno == ENOENT)
        {
            return;
        }
        ERR("lstat");
    }

    pthread_mutex_lock(&l->mtx);
    node_tr* current = l->head;

    while (current != NULL)
    {
        char* destination_path = concat(2, current->target_full, file_suffix);
        if (destination_path == NULL)
        {
            current = current->next;
            continue;
        }

        if (S_ISDIR(st.st_mode))
        {
            make_path(destination_path);
            checked_mkdir(destination_path);
            copy_permissions_and_attributes(source_path, destination_path);
        }
        else
        {
            copy(source_path, destination_path, source_full, current->target_full);
        }

        free(destination_path);
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
}

int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw)
{
    backup_ctx_lock();
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
        char* suf = get_end_suffix(_source, path);
        copy_to_all_targets(path, suf, &_source_node->targets, _source_node->source_full);
        free(suf);
    }
    else if (flag == FTW_SL)
    {
        copy_link(path, path_new);
    }
    free(path_new);
    backup_ctx_unlock();
    return 0;
}
