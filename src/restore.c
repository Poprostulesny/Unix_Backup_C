#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "restore.h"
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
#include "utils.h"
#include "generic_file_functions.h"
#include "inotify_functions.h"
#include <string.h>

char * _source_path;
char * _target_path;
//przechodzi przez backup i kopiuje zmienione/brakujące pliki do source
static int restorer(const char* path, const struct stat* s, int flag, struct FTW* ftw){
        char * suf = get_end_suffix(_target_path, path);
        char * dest_path = concat(2, _source_path, suf);
        struct stat st;
        int dst_exists = lstat(dest_path, &st);

        if (flag == FTW_D)
        {
            if (dst_exists == 0 && !S_ISDIR(st.st_mode))
            {
                recursive_deleter(dest_path);
            }
            make_path(dest_path);
            checked_mkdir(dest_path);
            copy_permissions_and_attributes(path, dest_path);
        }
        else if (flag == FTW_F || flag == FTW_SL)
        {
            int need_copy = 0;
            if (dst_exists == -1 && errno == ENOENT)
            {
                need_copy = 1;
            }
            else if (dst_exists == 0)
            {
                if ((S_ISREG(s->st_mode) && !S_ISREG(st.st_mode)) || (S_ISLNK(s->st_mode) && !S_ISLNK(st.st_mode)))
                {
                    recursive_deleter(dest_path);
                    need_copy = 1;
                }
                else if (S_ISREG(s->st_mode))
                {
                    if (s->st_size != st.st_size || s->st_mtim.tv_sec != st.st_mtim.tv_sec || s->st_mtim.tv_nsec != st.st_mtim.tv_nsec)
                    {
                        need_copy = 1;
                    }
                }
                else if (S_ISLNK(s->st_mode))
                {
                    if (s->st_mtim.tv_sec != st.st_mtim.tv_sec || s->st_mtim.tv_nsec != st.st_mtim.tv_nsec)
                    {
                        need_copy = 1;
                    }
                }
            }

            if (need_copy)
            {
                make_path(dest_path);
                copy(path, dest_path, _target_path, _source_path);
            }
        }

        free(suf);
        free(dest_path);
        return 0;
}
//usuwa z source wpisy nieobecne w backupie
static int delete_missing(const char* path, const struct stat* s, int flag, struct FTW* ftw){
     char * suf = get_end_suffix(_source_path, path);
        if (suf == NULL)
        {
            return 0;
        }
        char * partner_in_backup = concat(2, _target_path, suf);
        struct stat st;

        if (strlen(suf) == 0)
        {
            free(suf);
            free(partner_in_backup);
            return 0;
        }

        if(lstat(partner_in_backup, &st)==-1 && errno==ENOENT){
            recursive_deleter((char*)path);
        }
        else if(lstat(partner_in_backup, &st)==0 && s->st_mode!=st.st_mode){
            recursive_deleter((char*)path);
        }



        free(suf);
        free(partner_in_backup);
        return 0;
}
void restore_checkpoint( char* source_path, char* target_path)
{
    if (source_path == NULL || target_path == NULL)
    {
        return;
    }
    _source_path = source_path;
    _target_path = target_path;

    _source = _target_path;
    _target = _source_path;
    nftw(_target_path, restorer, 1024, FTW_PHYS);

    _source = _source_path;
    _target = _target_path;

    nftw(_source_path, delete_missing, 1024, FTW_DEPTH | FTW_PHYS);
    _source = NULL;
    _target = NULL;
    _source_path = NULL;
    _target_path = NULL;

}
