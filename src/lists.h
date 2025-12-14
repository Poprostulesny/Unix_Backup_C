#ifndef LISTS_H
#define LISTS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node_target
{
    char* target_friendly;
    char* target_full;
    struct Node_target* next;
    struct Node_target* previous;
} node_tr;

typedef struct List_target
{
    struct Node_target* head;
    struct Node_target* tail;
    int size;
} list_tg;

typedef struct Node_source
{
    char* source_full;
    char* source_friendly;
    struct List_target targets;
    struct Node_source* next;
    struct Node_source* previous;
} node_sc;

typedef struct List_source
{
    struct Node_source* head;
    struct Node_source* tail;
    int size;
} list_sc;

typedef struct Node_wd
{
    struct Node_wd* next;
    struct Node_wd* prev;
    int wd;
    char* source_friendly;
    char* source_full;
    char* path;

}Node_wd;
typedef struct List_wd
{
    struct Node_wd* head;
    struct Node_wd* tail;
    int size;

}list_wd;

/* GLOBAL VARIABLES*/
extern list_sc backups;
extern int inotify_fd;
extern char * _source;
extern char * _target;
extern list_wd wd_list;
/*-------------------*/

/*

LIST FUNCTIONS

*/

// Helper function to delete a single target node
void delete_target_node(node_tr* node);

// Helper function to delete a single source node
void delete_source_node(node_sc* node);

// Function to add a target node at the beginning of target list
void list_target_add(list_tg* l, node_tr* new_node);

// Function to delete a target node by full target name
void list_target_delete(list_tg* l, char* target);

// Function to add a source node at the beginning of source list
void list_source_add(node_sc* new_node);

// Function to delete a source node by source name
void list_source_delete(char* source);

// Function to check whether an element is already a target by the friendly name
int find_element_by_target_help(list_tg* l, char* target);

// checking whether a given target is already a target by the friendly name
int find_element_by_target(char* target);

// Function to find element by friendly source name
node_sc* find_element_by_source(char* source);

// Function to delete a wd node by its wd
void delete_wd_node(int wd);

// Function to add a wd node, taking its source, friendly source, and path to the place the wd is watching
void add_wd_node(int wd, char* source_friendly, char* source_full, char* path);

// Function to delete all wd with the given source
void delete_all_wd_by_path(char * source_friendly);

// Function to find an element by wd
Node_wd* find_element_by_wd(int wd);


#endif /* LISTS_H */