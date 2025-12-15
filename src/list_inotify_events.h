#ifndef LIST_INOTIFY_EVENTS_H
#define LIST_INOTIFY_EVENTS_H

#include "lists_common.h"
#include "list_wd.h"

typedef struct Inotify_event_node
{
    struct Inotify_event_node* next;
    struct Inotify_event_node* prev;
    int wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char* name;
    char* full_path;
    char* full_path_dest;
} Ino_Node;

typedef struct Inotify_List
{
    pthread_mutex_t mtx;
    struct Inotify_event_node* head;
    struct Inotify_event_node* tail;
    int size;
} Ino_List;

// Function to add an inotify event to the end of the list
void add_inotify_event(struct inotify_event* event);

// Function to remove the inotify event from the front of the list
void remove_inotify_event();

#endif /* LIST_INOTIFY_EVENTS_H */
