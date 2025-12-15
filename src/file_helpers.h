#ifndef FILE_HELPERS_H
#define FILE_HELPERS_H

#include "file_utils_common.h"

void checked_mkdir(const char* path);
void make_path(char* path);
int is_empty_dir(char* path);
int min(int a, int b);
int is_target_in_source(char* source, char* target);
char* concat(int n, ...);
void copy_permissions_and_attributes(const char* source, const char* dest);

#endif /* FILE_HELPERS_H */
