#ifndef LISTS_H
#define LISTS_H

#include "lists_common.h"
#include "list_targets.h"
#include "list_sources.h"
#include "list_wd.h"
#include "list_init_backup.h"
#include "list_inotify_events.h"
#include "list_move_events.h"

/* GLOBAL VARIABLES*/
extern list_bck init_backup_tasks;
extern list_sc backups;
extern list_wd wd_list;
extern Ino_List inotify_events;
extern M_list move_events;
/*-------------------*/

// Initialize all global lists mutexes and reset their state
void init_lists(void);

#if 0

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

typedef struct Node_wd
{   
    
    struct Node_wd* next;
    struct Node_wd* prev;
    int wd;
    char* source_friendly;
    char* source_full;
    char* path_new;
    char* path;

}Node_wd;

typedef struct List_wd
{   
    pthread_mutex_t mtx;
    struct Node_wd* head;
    struct Node_wd* tail;
    int size;

}list_wd;

typedef struct Node_init
{
    struct Node_init* next;
    struct Node_init* prev;
    char * source_full;
    char* target_full;
    char* source_friendly;

}Node_init;

typedef struct Init_backup_task_list
{   
    pthread_mutex_t mtx;
    struct Node_init* head;
    struct Node_init* tail;
    int size;

}list_bck;

typedef struct Inotify_event_node
{   
    struct Inotify_event_node * next;
    struct Inotify_event_node * prev;
    int wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char* name;
    char * full_path;
    char* full_path_dest;

}Ino_Node;

typedef struct Inotify_List
{
    pthread_mutex_t mtx;
    struct Inotify_event_node * head;
    struct Inotify_event_node * tail;
    int size;
}Ino_List;

typedef struct Move_Node
{   
    uint32_t cookie;
    char * move_from;
    char * move_to;
    char * full_dest_path;
    struct Move_Node * next;
    struct Move_Node * prev;
    time_t token;
}M_node;
typedef struct Move_List
{
    pthread_mutex_t mtx;
    struct Move_Node* head;
    struct Move_Node* tail;
    int size;
}M_list;

/* GLOBAL VARIABLES*/
extern list_bck init_backup_tasks;
extern list_sc backups;
extern list_wd wd_list;
extern Ino_List inotify_events;
extern M_list move_events;
/*-------------------*/

// Initialize all global lists mutexes and reset their state
void init_lists(void);

/*
    TARGET AND SOURCE LISTS
*/

// Helper function to delete a single target node
void delete_target_node(node_tr* node);

// Helper function to delete a single source node
void delete_source_node(node_sc* node);

// Function to add a target node at the beginning of target list
void list_target_add(list_tg* l, node_tr* new_node);

// Function to delete a target node by full target name
void list_target_delete(list_tg* l, char* target);

// Function to add a source node at the beginning of source list
void list_source_add(node_sc* new_node);

// Function to delete a source node by source name
void list_source_delete(char* source);

// Function to check whether an element is already a target by the friendly name
int find_element_by_target_help(list_tg* l, char* target);

// checking whether a given target is already a target by the friendly name
int find_element_by_target(char* target);

// Function to find element by friendly source name
node_sc* find_element_by_source(char* source);

/*
    WD LIST
*/

// Function to delete a wd node by its wd
void delete_wd_node(int wd);

// Function to add a wd node, taking its source, friendly source, and path to the place the wd is watching
void add_wd_node(int wd, char* source_friendly, char* source_full, char* path_new, const char* path);

// Function to delete all wd with the given source
void delete_all_wd_by_path(char * source_friendly);

// Function to find an element by wd
Node_wd* find_element_by_wd(int wd);

/*
    INITIAL BACKUPS LIST
*/

// Function to add a backup job to a queue
void init_backup_add_job(char* source, char* source_friendly, char* target);

//Function to mark the backup job as done
void init_backup_job_done();

/*
    INOTIFY EVENT LIST
*/

// Function to add an inotify event to the end of the list
void add_inotify_event(struct inotify_event *event);

// Function to remove the inotify event from the front of the list
void remove_inotify_event();

/*
    MOVE EVENTS LIST
*/

// Function to handle a MOVED_FROM event; returns corresponding move_to if cookie matches, otherwise adds new node
char* add_move_from_event(int cookie, char* move_from, char* full_dest_path);

// Function to handle a MOVED_TO event; returns corresponding move_from if cookie matches, otherwise adds new node
char* add_move_to_event(int cookie, char* move_to, char* full_dest_path);

// Function to check and clean up expired move events
void check_move_events_list();

void delete_node(M_node * current, M_list * l);

#endif /* legacy lists block */

#endif /* LISTS_H */