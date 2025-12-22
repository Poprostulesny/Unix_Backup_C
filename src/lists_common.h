#ifndef LISTS_COMMON_H
#define LISTS_COMMON_H

#include <stdint.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <time.h>

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

/* Watch descriptor list node and list structures */
typedef struct Node_wd
{
    struct Node_wd *next;
    struct Node_wd *prev;
    int wd;
    char *source_friendly;
    char *source_full;
    char *path;
    char *suffix;
} Node_wd;

typedef struct List_wd
{
    struct Node_wd *head;
    struct Node_wd *tail;
    int size;
} list_wd;

/* Move events node and list structures */
typedef struct Move_Node
{
    uint32_t cookie;
    char *move_from;
    char *move_to;
    struct Node_source *source;
    struct Move_Node *next;
    struct Move_Node *prev;
    time_t token;
} M_node;

typedef struct Move_List
{
    struct Move_Node *head;
    struct Move_Node *tail;
    int size;
} M_list;

/* Inotify event node and list structures */
typedef struct Inotify_event_node
{
    struct Inotify_event_node *next;
    struct Inotify_event_node *prev;
    int wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char *name;
    char *full_path;
    char *suffix;
} Ino_Node;

typedef struct Inotify_List
{
    struct Inotify_event_node *head;
    struct Inotify_event_node *tail;
    int size;
} Ino_List;

/* Target list node and list structures */
typedef struct Node_target
{
    char *target_friendly;
    char *target_full;
    pid_t child_pid;
    int inotify_fd;
    struct List_wd watchers;
    struct Inotify_List events;
    struct Move_List mov_dict;
    struct Node_target *next;
    struct Node_target *previous;
} node_tr;

typedef struct List_target
{
    struct Node_target *head;
    struct Node_target *tail;
    int size;
} list_tg;

/* Initial backup task list node and list structures */
typedef struct Node_init
{
    struct Node_init *next;
    struct Node_init *prev;
    char *source_full;
    char *target_full;
    char *source_friendly;
} Node_init;

typedef struct Init_backup_task_list
{
    struct Node_init *head;
    struct Node_init *tail;
    int size;
} list_bck;
typedef struct Node_source
{
    char *source_full;
    char *source_friendly;
    struct List_target targets;
    struct Node_source *next;
    struct Node_source *previous;
} node_sc;

typedef struct List_source
{
    struct Node_source *head;
    struct Node_source *tail;
    int size;
} list_sc;
/* Global lists (defined in lists.c) */
extern list_sc backups;

// Initialize all global lists mutexes and reset their state
void init_lists(void);

#endif /* LISTS_COMMON_H */
