// src/inotify_functions.h
#ifndef INOTIFY_FUNCTIONS_H
#define INOTIFY_FUNCTIONS_H



#include <sys/inotify.h>
#include <sys/stat.h>
#include <ftw.h>
#include "lists_common.h"
#include "list_move_events.h"

//Function instantiating inotify recursively in a given source
int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw);

//handler of initial backup events. creates a inotify instance if necessary
void initial_backup(node_sc *source_node , char* source_friendly, char* target);

// Recursive initalization of a new folder in an inotify domain.
void new_folder_init(node_sc* source_node, char* path);

//Reads inotify events from a given fd
void inotify_reader(int fd, list_wd* wd_list, Ino_List* inotify);

//Handles events for a source node
void event_handler(node_sc *source_node);


#endif /* INOTIFY_FUNCTIONS_H */

