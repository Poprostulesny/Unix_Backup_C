#ifndef DELETION_HELPERS_H
#define DELETION_HELPERS_H

#include "file_utils_common.h"

int deleter(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void recursive_deleter(char* path);

#endif /* DELETION_HELPERS_H */
