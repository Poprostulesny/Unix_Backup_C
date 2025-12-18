
#ifndef DEBUG
// #define DEBUG
#endif

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
#include "lists_common.h"

/* GLOBAL VARIABLES */
extern char* _source;
extern char* _source_friendly;
extern char* _target;
/*-------------------*/
//Functions from labs
void checked_mkdir(const char* path);
void make_path(char* path);

//Checks whether the given directory is empty return -1 if it is not a directory or is not empty
int is_empty_dir(char* path);

//Copies permissions and attributes between two files
void copy_permissions_and_attributes(const char* source, const char* dest);
//From source and a path of the file in source it gets the path to the same file in target
char* get_path_to_target(const char* source, const char* target, const char* path);

void copy_file(const char* path1, const char* path2);
void copy_link(const char* path_where, const char* path_dest);
//wrapper for the above two functions
void copy(const char* source, const char * dest,  char *  backup_source, char * backup_target);

//Copies all files from a given directory to a target, without instantiating inotify fd. Separation between it and the one activating inotify was needed in a multithreaded implementatnion
int backup_walk(const char* path, const struct stat* s, int flag, struct FTW* ftw);

// helper for recursive_deleter
int deleter(const char* path, const struct stat* s, int flag, struct FTW* ftw);
// recursively deletes all files in a given directory
void recursive_deleter(char* path);

//Handles the deletion of files and directories
void del_handling(const char* dest_path);

//Wrappers to the above functions for use in a no longer existant function for_all_target_paths
void create_empty_files(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
void copy_files(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
void attribs(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
void delete_multi(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
void move_all(const char* dest_path, node_tr* target, node_sc* source_node, void* ctx);
#endif /* GENERIC_FILE_FUNCTIONS_H */
