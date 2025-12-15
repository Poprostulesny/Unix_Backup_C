#ifndef LIST_WD_H
#define LIST_WD_H

#include "lists_common.h"

typedef struct Node_wd
{
    struct Node_wd* next;
    struct Node_wd* prev;
    int wd;
    char* source_friendly;
    char* source_full;
    char* path_new;
    char* path;
} Node_wd;

typedef struct List_wd
{
    pthread_mutex_t mtx;
    struct Node_wd* head;
    struct Node_wd* tail;
    int size;
} list_wd;

// Function to delete a wd node by its wd
void delete_wd_node(int wd);

// Function to add a wd node, taking its source, friendly source, and path to the place the wd is watching
void add_wd_node(int wd, char* source_friendly, char* source_full, char* path_new, const char* path);

// Function to delete all wd with the given source
void delete_all_wd_by_path(char* source_friendly);

// Function to find an element by wd
Node_wd* find_element_by_wd(int wd);

#endif /* LIST_WD_H */
