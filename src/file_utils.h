#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include "file_utils_common.h"
#include "file_helpers.h"
#include "backup_init.h"
#include "deletion_helpers.h"
#include "inotify_helpers.h"
#include "inotify_event_handler.h"
/* GLOBAL VARIABLES */
extern char* _source;
extern char* _source_friendly;
extern char* _target;
extern int fd;
/*-------------------*/

#if 0
/* legacy declarations retained for reference */
void checked_mkdir(const char* path);
void make_path(char* path);
int is_empty_dir(char* path);
int min(int a, int b);
int is_target_in_source(char* source, char* target);
char* concat(int n, ...);
void copy_permissions_and_attributes(const char* source, const char* dest);
char* get_path_to_target(const char* source, const char* target, const char* path);
void copy_file(const char* path1, const char* path2);
void copy_link(const char* path_where, const char* path_dest);
int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw);
int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void initial_backup(char* source, char* source_friendly, char* target);
int deleter(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void recursive_deleter(char* path);
void create_watcher(char* source, char* target);
void new_folder_init(char* source, char* source_friendly, char* path);
void inotify_reader();
void event_handler();
#endif
#endif /* FILE_UTILS_H */
