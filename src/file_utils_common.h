#ifndef FILE_UTILS_COMMON_H
#define FILE_UTILS_COMMON_H

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
#include <sys/inotify.h>
#include "lists.h"

#define INOTIFY_MASK (IN_ATTRIB|IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_DELETE_SELF|IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO|IN_IGNORED)
#define BUF_SIZE 4096

#endif /* FILE_UTILS_COMMON_H */
