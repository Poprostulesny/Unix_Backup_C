#ifndef LIST_MOVE_EVENTS_H
#define LIST_MOVE_EVENTS_H

#include "lists_common.h"

typedef struct Move_Node
{
    uint32_t cookie;
    char* move_from;
    char* move_to;
    char* full_dest_path;
    struct Move_Node* next;
    struct Move_Node* prev;
    time_t token;
} M_node;

typedef struct Move_List
{
    pthread_mutex_t mtx;
    struct Move_Node* head;
    struct Move_Node* tail;
    int size;
} M_list;

char* add_move_from_event(int cookie, char* move_from, char* full_dest_path);
char* add_move_to_event(int cookie, char* move_to, char* full_dest_path);
void check_move_events_list();
void delete_node(M_node* current, M_list* l);

#endif /* LIST_MOVE_EVENTS_H */
