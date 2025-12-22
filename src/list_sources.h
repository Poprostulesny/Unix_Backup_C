#ifndef LIST_SOURCES_H
#define LIST_SOURCES_H

#include "list_targets.h"
#include "lists_common.h"

// Function to add a source node at the beginning of source list
void list_source_add(node_sc *new_node);

// Function to delete a source node by source name
void list_source_delete(char *source);

// Helper function to delete a single source node
void delete_source_node(node_sc *node);

// checking whether a given target is already a target by the friendly name
int find_element_by_target(char *target);

// Function to find element by friendly source name
node_sc *find_element_by_source(char *source);

// Function to get source_friendly by source_full
char *get_source_friendly(char *source_full);

#endif /* LIST_SOURCES_H */
