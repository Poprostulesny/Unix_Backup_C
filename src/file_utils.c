#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ftw.h>
#include "file_utils.h"
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

//returns 1 if target is in source, 0 otherwise
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
/*-----------------------------------------------------*/

/*

    INITIAL BACKUPS

*/
/*Function that gets the full path for source and target, and current whole path, and returns the path from target to
 * path in target */
char* get_path_to_target(const char* source, const char* target,  const char* path)
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
void copy_file(const char* path1, const char* path2){
    #ifdef DEBUG
        printf("Copying file %s to %s\n", path1, path2);
    #endif
   
    int read_fd = open(path1, O_RDONLY);
    int write_fd = open(path2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(read_fd<0){
        ERR("read fd");
    }
    if(write_fd<0){
        ERR("write fd");
    }
    char buff[1024];
    int r=0;
    while((r=read(read_fd, buff, sizeof(buff)))>0){
        ssize_t offset =0;
        while(offset<r){
            ssize_t w = write(write_fd, buff+offset, r-offset);
            if(w<0){
                close(read_fd);
                close(write_fd);
                ERR("copy file");
            }
            offset+=w;
        }

    }        
    close(read_fd);
    close(write_fd);
    if(r<0){
        ERR("copy file");
    }

    
}
void copy_link(const char * path_where, const char * path_dest){
    #ifdef DEBUG
          printf("Copying symlink %s to %s\n", path_where, path_dest);
    #endif
 
    char buff[1024];
    ssize_t l = readlink(path_where, buff, sizeof(buff)-1);
    if(l<0){
        ERR("readlink");
    }
    buff[l]='\0';
    if(is_target_in_source(_source, buff)==1){
        char * path_updated = get_path_to_target(_source, _target, buff);
        if(symlink( path_updated, path_dest)<0){
            ERR("symlink");
        }
        free(path_updated);
    }
    else{
        if(symlink( buff, path_dest)<0){
            ERR("symlink");
        }
    }
}

int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw)
{
    char * path_new = get_path_to_target(_source,_target, path);
    if (path_new == NULL) {
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
        copy_link(path,path_new);
    }
    free(path_new);
    return 0;
}
int backup_walk_inotify_init(const char * path, const struct stat* s, int flag, struct FTW* ftw){

    char * path_new = get_path_to_target(_source,_target, path);
    if (path_new == NULL) {
        ERR("get_path_to_target");
    }
    if (flag == FTW_D)
    {
        checked_mkdir(path_new);
        int wd = inotify_add_watch(fd, path,INOTIFY_MASK );
        if(wd==-1){
            ERR("inotify_add_watch");
        }
        add_wd_node(wd, _source_friendly, _source, path_new);
    }
    else if (flag == FTW_F)
    {   
        copy_file(path, path_new);
    }

    else if (flag == FTW_SL)
    {
        copy_link(path,path_new);
    }
    free(path_new);
    return 0;





}

void initial_backup(char* source, char* target) { 
    #ifdef DEBUG
        printf("Doing an initial backup of %s to %s...\n", source, target);
    #endif
   
    _source = strdup(source);

    _source_friendly = get_source_friendly(source);

    _target = strdup(target);
    if(_source==NULL||_target==NULL){
        ERR("strdup");
    }
    nftw(source, backup_walk_inotify_init, 1024, FTW_PHYS); 
    free(_source);
    free(_target);
    #ifdef DEBUG
          printf("Finished\n");
    #endif
 
}

/* 

    INOTIFY EVENT HANDLING

*/

void create_watcher(char * source, char * target){
    


}

void inotify_reader(){
    char buffer[BUF_SIZE];
    ssize_t bytes_read=0;

    while (1) {
        bytes_read = read(fd, buffer, BUF_SIZE);
        
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            } 
            else {
                ERR("read");
                break;
            }
        } 
        else if (bytes_read > 0) {
            
            size_t offset = 0;
            //Computing every struct in the buffer
            while (offset < (size_t)bytes_read) {
                struct inotify_event *event = (struct inotify_event *)(buffer + offset);
                
                // Is there enough data to fill the struct
                if (offset + sizeof(struct inotify_event) > (size_t)bytes_read) {
                    fprintf(stderr, "Partial header read!\n");
                    break;
                }
                
                // Is there enough space for the name
                size_t event_size = sizeof(struct inotify_event) + event->len;
                if (offset + event_size > (size_t)bytes_read) {
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

        
    }
    free(buffer);
    


}