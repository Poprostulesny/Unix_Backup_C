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

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

/* GLOBAL VARIABLES */
extern char* _source;
extern char* _target;
/*-------------------*/

/*
    HELPER FUNCTIONS
*/
void checked_mkdir(const char* path);
void make_path(char* path);
int is_empty_dir(char* path);
int min(int a, int b);
int is_target_in_source(char* source, char* target);

/*
    INITIAL BACKUPS
*/
char* get_path_to_target(const char* source, const char* target, const char* path);
void copy_file(const char* path1, const char* path2);
void copy_link(const char* path_where, const char* path_dest);
int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void initial_backup(char* source, char* target);

/*
    INOTIFY EVENT HANDLING
*/

#endif /* FILE_UTILS_H */
