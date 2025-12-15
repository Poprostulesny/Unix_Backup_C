#ifndef LIST_SOURCES_H
#define LIST_SOURCES_H

#include "lists_common.h"
#include "list_targets.h"

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
    pthread_mutex_t mtx;
    struct Node_source* head;
    struct Node_source* tail;
    int size;
} list_sc;

// Helper function to delete a single source node
void delete_source_node(node_sc* node);

// Function to add a source node at the beginning of source list
void list_source_add(node_sc* new_node);

// Function to delete a source node by source name
void list_source_delete(char* source);

// checking whether a given target is already a target by the friendly name
int find_element_by_target(char* target);

// Function to find element by friendly source name
node_sc* find_element_by_source(char* source);

#endif /* LIST_SOURCES_H */
