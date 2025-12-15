#ifndef INOTIFY_FUNCTIONS_H
#define INOTIFY_FUNCTIONS_H

#include <sys/inotify.h>
#include <sys/stat.h>
#include <ftw.h>

int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void initial_backup(char* source, char* source_friendly, char* target);
int deleter(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void recursive_deleter(char* path);
void create_watcher(char* source, char* target);
void new_folder_init(char* source, char* source_friendly, char* path);
void inotify_reader(void);
void event_handler(void);
void del_handling(void* event);

#endif /* INOTIFY_FUNCTIONS_H */
