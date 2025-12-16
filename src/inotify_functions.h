#ifndef DEBUG
#define DEBUG
#endif

#ifndef INOTIFY_FUNCTIONS_H
#define INOTIFY_FUNCTIONS_H



#include <sys/inotify.h>
#include <sys/stat.h>
#include <ftw.h>
#include "lists_common.h"

/* GLOBAL VARIABLES */
extern int _fd;
/*-------------------*/

int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void initial_backup(char* source, char* source_friendly, char* target);
// helper for recursive_deleter
int deleter(const char* path, const struct stat* s, int flag, struct FTW* ftw);
//recursively deletes all files in a given directory
void recursive_deleter(char* path);
//just exists dont worry abt it
void create_watcher(char* source, char* target);

void new_folder_init(char* source, char* source_friendly, char* path, char * target, int fd);
void inotify_reader(int fd, list_wd* wd_list, Ino_List* inotify);
void event_handler(list_wd* wd_list, Ino_List* inotify);
void del_handling(void* event);

#endif /* INOTIFY_FUNCTIONS_H */
