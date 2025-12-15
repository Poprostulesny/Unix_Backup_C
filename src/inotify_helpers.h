#ifndef INOTIFY_HELPERS_H
#define INOTIFY_HELPERS_H

#include "file_utils_common.h"

void create_watcher(char* source, char* target);
void new_folder_init(char* source, char* source_friendly, char* path);
void inotify_reader();

#endif /* INOTIFY_HELPERS_H */
