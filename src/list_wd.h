#ifndef LIST_WD_H
#define LIST_WD_H

#include "lists_common.h"

// Function to delete a wd node by its wd
void delete_wd_node(int wd);

// Function to add a wd node, taking its source, friendly source, and path to the place the wd is watching
void add_wd_node(int wd, char* source_friendly, char* source_full, char* path_new, const char* path);

// Function to find an element by wd
Node_wd* find_element_by_wd(int wd);

// Function to delete all wd with the given source
void delete_all_wd_by_path(char * source_friendly);

#endif /* LIST_WD_H */
