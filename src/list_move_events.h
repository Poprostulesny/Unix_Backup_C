#ifndef LIST_MOVE_EVENTS_H
#define LIST_MOVE_EVENTS_H

#include "inotify_functions.h"
#include "lists_common.h"
#include "utils.h"

// Unified move event handler; is_from=1 for MOVED_FROM, 0 for MOVED_TO
void add_move_event(M_list *l, uint32_t cookie, const char *path, int is_from, node_sc *source, node_tr *target);

// Function to check and clean up expired move events
void check_move_events_list(M_list *l, node_sc *source, node_tr *target);

// Helper function to delete a move node
void delete_node(M_list *l, M_node *current);

#endif /* LIST_MOVE_EVENTS_H */
