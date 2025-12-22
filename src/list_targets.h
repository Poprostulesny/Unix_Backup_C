#ifndef LIST_TARGETS_H
#define LIST_TARGETS_H
#include "lists_common.h"

// Helper function to delete a single target node
void delete_target_node(node_tr *node);

// Function to add a target node at the beginning of target list
void list_target_add(list_tg *l, node_tr *new_node);

// Function to delete a target node by full target name; returns 1 on delete, 0
// if not found
int list_target_delete(list_tg *l, char *target);

// Function to check whether an element is already a target by the friendly name
int find_element_by_target_help(list_tg *l, char *target);

#endif /* LIST_TARGETS_H */
