#ifndef FILE_UTILS_H
#define FILE_UTILS_H

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
#include <stdarg.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define INOTIFY_MASK (IN_ATTRIB|IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_DELETE_SELF|IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO|IN_IGNORED)
#define BUF_SIZE 4096
/* GLOBAL VARIABLES */
extern char* _source;
extern char* _source_friendly;
extern char* _target;
extern int fd;
/*-------------------*/

/*
    HELPER FUNCTIONS
*/
void checked_mkdir(const char* path);
void make_path(char* path);
int is_empty_dir(char* path);
int min(int a, int b);
int is_target_in_source(char* source, char* target);
char* concat(int n, ...);
void copy_permissions_and_attributes(const char* source, const char* dest);

/*
    INITIAL BACKUPS
*/
char* get_path_to_target(const char* source, const char* target, const char* path);
void copy_file(const char* path1, const char* path2);
void copy_link(const char* path_where, const char* path_dest);
int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw);
int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void initial_backup(char* source, char* source_friendly, char* target);

/*
    DELETION HELPERS
*/
int deleter(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void recursive_deleter(char* path);

/*
    INOTIFY HELPERS
*/
void create_watcher(char* source, char* target);
void new_folder_init(char* source, char* source_friendly, char* path);
void inotify_reader();

/*
    INOTIFY EVENT HANDLING
*/
void event_handler();
#endif /* FILE_UTILS_H */
