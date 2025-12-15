#ifndef LIST_TARGETS_H
#define LIST_TARGETS_H

#include "lists_common.h"

typedef struct Node_target
{
    char* target_friendly;
    char* target_full;
    struct Node_target* next;
    struct Node_target* previous;
} node_tr;

typedef struct List_target
{
    pthread_mutex_t mtx;
    struct Node_target* head;
    struct Node_target* tail;
    int size;
} list_tg;

// Helper function to delete a single target node
void delete_target_node(node_tr* node);

// Function to add a target node at the beginning of target list
void list_target_add(list_tg* l, node_tr* new_node);

// Function to delete a target node by full target name
void list_target_delete(list_tg* l, char* target);

// Function to check whether an element is already a target by the friendly name
int find_element_by_target_help(list_tg* l, char* target);

#endif /* LIST_TARGETS_H */
