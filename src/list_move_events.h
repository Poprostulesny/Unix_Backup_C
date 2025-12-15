#ifndef LIST_MOVE_EVENTS_H
#define LIST_MOVE_EVENTS_H

#include "lists_common.h"

// Function to handle a MOVED_FROM event; returns corresponding move_to if cookie matches, otherwise adds new node
char* add_move_from_event(int cookie, char* move_from, char* full_dest_path);

// Function to handle a MOVED_TO event; returns corresponding move_from if cookie matches, otherwise adds new node
char* add_move_to_event(int cookie, char* move_to, char* full_dest_path);

// Function to check and clean up expired move events
void check_move_events_list(void);

// Helper function to delete a move node
void delete_node(M_node * current, M_list * l);

#endif /* LIST_MOVE_EVENTS_H */
