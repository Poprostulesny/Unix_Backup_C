#ifndef GENERIC_FILE_FUNCTIONS_H
#define GENERIC_FILE_FUNCTIONS_H

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void checked_mkdir(const char* path);
void make_path(char* path);
int is_empty_dir(char* path);
void copy_permissions_and_attributes(const char* source, const char* dest);
char* get_path_to_target(const char* source, const char* target, const char* path);
void copy_file(const char* path1, const char* path2);
void copy_link(const char* path_where, const char* path_dest);
int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw);

#endif /* GENERIC_FILE_FUNCTIONS_H */
