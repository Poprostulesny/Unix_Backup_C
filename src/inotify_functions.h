// src/inotify_functions.h
#ifndef INOTIFY_FUNCTIONS_H
#define INOTIFY_FUNCTIONS_H



#include <sys/inotify.h>
#include <sys/stat.h>
#include <ftw.h>
#include "lists_common.h"
#include "list_move_events.h"

int backup_walk_inotify_init(const char* path, const struct stat* s, int flag, struct FTW* ftw);
void initial_backup(node_sc *source_node , char* source_friendly, char* target);
// helper for recursive_deleter
int deleter(const char* path, const struct stat* s, int flag, struct FTW* ftw);
//recursively deletes all files in a given directory
void recursive_deleter(char* path);
//just exists dont worry abt it
void create_watcher(char* source, char* target);

void new_folder_init(node_sc* source_node, char* path);
void inotify_reader(int fd, list_wd* wd_list, Ino_List* inotify);
void event_handler(node_sc *source_node);
void del_handling(const char* dest_path);


void for_each_target_path(node_sc* source_node, const char* suffix, void (*f)(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx), void* ctx);
void create_empty_files(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
void copy_files(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
void attribs(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
void delete_multi(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
void move_all(const char*dest_path, node_tr * target, node_sc * source_node, void *ctx);
#endif /* INOTIFY_FUNCTIONS_H */
