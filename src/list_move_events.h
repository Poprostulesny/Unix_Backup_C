#ifndef LIST_MOVE_EVENTS_H
#define LIST_MOVE_EVENTS_H

#include "lists_common.h"

// Function to handle a MOVED_FROM event; returns corresponding move_to if cookie matches, otherwise adds new node
char* add_move_from_event(M_list* l, int cookie, char* move_from);

// Function to handle a MOVED_TO event; returns corresponding move_from if cookie matches, otherwise adds new node
char* add_move_to_event(M_list* l, int cookie, char* move_to, int wd_to);

// Function to check and clean up expired move events
void check_move_events_list(M_list* l);

// Helper function to delete a move node
void delete_node(M_list* l, M_node * current);

#endif /* LIST_MOVE_EVENTS_H */
