#ifndef BACKUP_INIT_H
#define BACKUP_INIT_H

#include "file_utils_common.h"

char* get_path_to_target(const char* source, const char* target, const char* path);
void copy_file(const char* path1, const char* path2);
void copy_link(const char* path_where, const char* path_dest);
int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw);
int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void initial_backup(char* source, char* source_friendly, char* target);

#endif /* BACKUP_INIT_H */
