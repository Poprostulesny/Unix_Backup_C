#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "lists.h"


/* GLOBAL VARIABLES*/
list_bck init_backup_tasks;
list_sc backups;
list_wd wd_list;
Ino_List inotify_events;
M_list move_events;
/*-------------------*/

void init_lists(void)
{
    // Backups list
    backups.head = NULL;
    backups.tail = NULL;
    backups.size = 0;
    if (pthread_mutex_init(&backups.mtx, NULL) != 0)
    {
        ERR("pthread_mutex_init backups");
    }

    // Watch descriptors list
    wd_list.head = NULL;
    wd_list.tail = NULL;
    wd_list.size = 0;
    if (pthread_mutex_init(&wd_list.mtx, NULL) != 0)
    {
        ERR("pthread_mutex_init wd_list");
    }

    // Initial backup tasks queue
    init_backup_tasks.head = NULL;
    init_backup_tasks.tail = NULL;
    init_backup_tasks.size = 0;
    if (pthread_mutex_init(&init_backup_tasks.mtx, NULL) != 0)
    {
        ERR("pthread_mutex_init init_backup_tasks");
    }

    // Inotify events queue
    inotify_events.head = NULL;
    inotify_events.tail = NULL;
    inotify_events.size = 0;
    if (pthread_mutex_init(&inotify_events.mtx, NULL) != 0)
    {
        ERR("pthread_mutex_init inotify_events");
    }
    
    //Move events dict
    move_events.head=NULL;
    move_events.tail=NULL;
    move_events.size=0;
    if(pthread_mutex_init(&move_events.mtx, NULL)!=0){  
        ERR("pthread_mutex_init move_events");

    }
}

/*

LIST FUNCTIONS

*/

// Helper function to delete a single target node
void delete_target_node(node_tr* node)
{
    if (node != NULL)
    {
        free(node->target_friendly);
        free(node->target_full);
        free(node);
    }
}

// Helper function to delete a single source node
void delete_source_node(node_sc* node)
{
    if (node != NULL)
    {
        free(node->source_friendly);
        free(node->source_full);
        pthread_mutex_lock(&node->targets.mtx);
        node_tr* current_target = node->targets.head;
        while (current_target != NULL)
        {
            node_tr* temp = current_target;
            current_target = current_target->next;
            delete_target_node(temp);
        }
        pthread_mutex_unlock(&node->targets.mtx);
        pthread_mutex_destroy(&node->targets.mtx);
        free(node);
    }
}

// Function to add a target node at the beginning of target list
void list_target_add(list_tg* l, node_tr* new_node)
{
    if (l == NULL || new_node == NULL)
    {
        return;
    }

    pthread_mutex_lock(&l->mtx);

    new_node->next = l->head;
    new_node->previous = NULL;

    if (l->head != NULL)
    {
        l->head->previous = new_node;
    }
    else
    {
        l->tail = new_node;
    }

    l->head = new_node;
    l->size++;

    pthread_mutex_unlock(&l->mtx);
}

// Function to delete a target node by full target name
void list_target_delete(list_tg* l, char* target)
{
    if (l == NULL || target == NULL)
    {
        return;
    }

    pthread_mutex_lock(&l->mtx);
    node_tr* current = l->head;
    while (current != NULL)
    {
        if (strcmp(current->target_friendly, target) == 0)
        {
            // Found the node to delete
            if (current->previous != NULL)
            {
                current->previous->next = current->next;
            }
            else
            {
                l->head = current->next;
            }

            if (current->next != NULL)
            {
                current->next->previous = current->previous;
            }
            else
            {
                l->tail = current->previous;
            }

            delete_target_node(current);
            #ifdef DEBUG
        printf("Deleted node with full path: %s\n", current->target_full);
            #endif
           
            l->size--;
            pthread_mutex_unlock(&l->mtx);
            return;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
    printf("Target not found: %s\n", target);
}

// Function to add a source node at the beginning of source list
void list_source_add(node_sc* new_node)
{
    list_sc* l = &backups;
    if (l == NULL || new_node == NULL)
    {
        return;
    }
    #ifdef DEBUG
        fprintf(stderr, "ADD node_sc %p (source=%s)\n", (void*)new_node, new_node->source_friendly);
    #endif
   
    pthread_mutex_lock(&l->mtx);
    new_node->next = l->head;
    new_node->previous = NULL;

    if (l->head != NULL)
    {
        l->head->previous = new_node;
    }
    else
    {
        l->tail = new_node;
    }

    l->head = new_node;
    l->size++;
    pthread_mutex_unlock(&l->mtx);
}

// Function to delete a source node by source name
void list_source_delete(char* source)
{
    list_sc* l = &backups;
    if (l == NULL || source == NULL)
    {
        return;
    }

    pthread_mutex_lock(&l->mtx);
    node_sc* current = l->head;
    #ifdef DEBUG
          fprintf(stderr, "DEL node_sc %p (source=%s)\n", (void*)current, current->source_friendly);
    #endif
 

    while (current != NULL)
    {
        if (strcmp(current->source_friendly, source) == 0)
        {
            if (current->previous != NULL)
            {
                current->previous->next = current->next;
            }
            else
            {
                l->head = current->next;
            }

            if (current->next != NULL)
            {
                current->next->previous = current->previous;
            }
            else
            {
                l->tail = current->previous;
            }

            l->size--;
            pthread_mutex_unlock(&l->mtx);
            delete_source_node(current);
            return;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
}

// Function to check whether an element is already a target by the friendly name
int find_element_by_target_help(list_tg* l, char* target)
{
    if (l == NULL || target == NULL)
    {
        return 0;
    }

    pthread_mutex_lock(&l->mtx);
    node_tr* current = l->head;
    while (current != NULL)
    {
        if (strcmp(current->target_friendly, target) == 0)
        {
            pthread_mutex_unlock(&l->mtx);
            return 1;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
    return 0;
}

// checking whether a given target is already a target by the friendly name
int find_element_by_target(char* target)
{
    list_sc* l = &backups;

    if (l == NULL || target == NULL)
    {
        return 0;
    }
    pthread_mutex_lock(&l->mtx);
    node_sc* current = l->head;
    while (current != NULL)
    {
        if (find_element_by_target_help(&current->targets, target) == 1)
        {
            pthread_mutex_unlock(&l->mtx);
            return 1;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
    return 0;
}

// Function to find element by friendly source name
node_sc* find_element_by_source(char* source)
{
    list_sc* l = &backups;
    if (l == NULL || source == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&l->mtx);
    node_sc* current = l->head;
    while (current != NULL)
    {
        if (current->source_friendly == NULL)
        {   
            #ifdef DEBUG
         fprintf(stderr, "BUG: current=%p, current->source == NULL (list corrupted)\n", (void*)current);
            #endif
          
            abort();
        }
        if (strcmp(current->source_friendly, source) == 0)
        {
            pthread_mutex_unlock(&l->mtx);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
    return NULL;
}

// Functions for wd list
void delete_wd_node(int wd)
{
    list_wd* l = &wd_list;
    if (l == NULL)
    {
        return;
    }
    pthread_mutex_lock(&l->mtx);
    Node_wd* current = l->head;
    while (current != NULL)
    {
        if (current->wd == wd)
        {
           
            if (current->prev != NULL)
            {
                current->prev->next = current->next;
            }
            else
            {
                l->head = current->next;
            }

            if (current->next != NULL)
            {
                current->next->prev = current->prev;
            }
            else
            {
                l->tail = current->prev;
            }

            free(current->source_friendly);
            free(current->source_full);
            free(current->path);
            free(current);
            l->size--;
            pthread_mutex_unlock(&l->mtx);
            return;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
}

void add_wd_node(int wd, char* source_friendly, char* source_full, char* path_new, const char* path)
{
    list_wd* l = &wd_list;
    if (l == NULL)
    {
        return;
    }

    Node_wd* new_node = malloc(sizeof(Node_wd));
    if (new_node == NULL)
    {
        return;
    }

    new_node->wd = wd;
    new_node->source_friendly = strdup(source_friendly);
    new_node->source_full = strdup(source_full);
    new_node->path = strdup(path);
    new_node->path_new= strdup(path_new);
    new_node->next = NULL;
    if(  new_node->source_friendly==NULL|| new_node->source_full==NULL||new_node->path==NULL||new_node->path_new==NULL){
        ERR("strdup");
    }
    pthread_mutex_lock(&l->mtx);
    new_node->prev = l->tail;

    if (l->tail != NULL)
    {
        l->tail->next = new_node;
    }
    else
    {
        l->head = new_node;
    }

    l->tail = new_node;
    l->size++;
    pthread_mutex_unlock(&l->mtx);
}

Node_wd* find_element_by_wd(int wd)
{
    list_wd* l = &wd_list;
    if (l == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&l->mtx);
    Node_wd* current = l->head;
    while (current != NULL)
    {
        if (current->wd == wd)
        {
            pthread_mutex_unlock(&l->mtx);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
    return NULL;
}


void delete_all_wd_by_path(char * source_friendly){
    list_wd* l = &wd_list;
    if (l == NULL)
    {
        return;
    }

    pthread_mutex_lock(&l->mtx);
    Node_wd* current = l->head;
    Node_wd* next;
    while (current != NULL)
    {       
        next=current->next;
        if (strcmp(current->source_friendly, source_friendly)==0)
        {   

            if (current->prev != NULL) {
                current->prev->next = current->next;
            } 
            else {
                l->head = current->next;
            }
            if (current->next != NULL) {
                current->next->prev = current->prev;
            } 
            else {
                l->tail = current->prev;
            }
            free(current->source_friendly);
            free(current->source_full);
            free(current->path);
            free(current->path_new);
            free(current);
            l->size--;

        }
        current = next;
    }
    pthread_mutex_unlock(&l->mtx);
    

}

// Functions for init backup tasks
void init_backup_add_job(char* source, char* source_friendly, char* target)
{
    list_bck* l = &init_backup_tasks;
    if (l == NULL || source == NULL || source_friendly == NULL || target == NULL)
    {
        return;
    }

    Node_init* new_node = malloc(sizeof(Node_init));
    if (new_node == NULL)
    {
        return;
    }

    new_node->source_full = strdup(source);
    new_node->source_friendly = strdup(source_friendly);
    new_node->target_full = strdup(target);
    new_node->next = NULL;

    pthread_mutex_lock(&l->mtx);
    new_node->prev = l->tail;  

    if (l->tail != NULL)
    {
        l->tail->next = new_node;
    }
    else
    {
        l->head = new_node;
    }

    l->tail = new_node;
    l->size++;
    pthread_mutex_unlock(&l->mtx);
}

void init_backup_job_done()
{
    list_bck* l = &init_backup_tasks;
    if (l == NULL || l->head == NULL)
    {
        return;
    }

    pthread_mutex_lock(&l->mtx);
    Node_init* to_delete = l->head;
    l->head = to_delete->next;
    if (l->head != NULL)
    {
        l->head->prev = NULL;
    }
    else
    {
        l->tail = NULL;
    }

    l->size--;
    pthread_mutex_unlock(&l->mtx);
    free(to_delete->source_full);
    free(to_delete->source_friendly);
    free(to_delete->target_full);
    free(to_delete);
}

// Function to get source_friendly by source_full
char* get_source_friendly(char* source_full)
{
    list_sc* l = &backups;
    if (l == NULL || source_full == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&l->mtx);
    node_sc* current = l->head;
    while (current != NULL)
    {
        if (strcmp(current->source_full, source_full) == 0)
        {
            char* ret = strdup(current->source_friendly);
            pthread_mutex_unlock(&l->mtx);
            return ret;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
    return NULL;
}



void add_inotify_event(struct inotify_event *event)
{
    Ino_List *l = &inotify_events;
    if (l == NULL || event == NULL)
    {
        return;
    }

    Node_wd *wd_node = find_element_by_wd(event->wd);

    Ino_Node *new_node = malloc(sizeof(Ino_Node));
    if (new_node == NULL)
    {
        ERR("malloc");
    }

    new_node->wd = event->wd;
    new_node->mask = event->mask;
    new_node->cookie = event->cookie;
    new_node->len = event->len;
    
    
    new_node->name = strdup(event->name);
    if(new_node->name==NULL){
        ERR("strdup");
    }
   

    //creating full path to the element
    size_t path_len = strlen(wd_node->path);
    size_t path_dest_len = strlen(wd_node->path_new);
    size_t name_len;
    if (event->len > 0) {
        name_len = strlen(event->name);
    } else {
        name_len = 0;
    }

    size_t full_len = path_len;
    size_t full_dest_len = path_dest_len;
    if (name_len > 0) {
        full_len += 1 + name_len;
        full_dest_len+=1+name_len;
    }
    full_dest_len+=1;
    full_len += 1; //adding '/'
    new_node->full_path = malloc(full_len);
    new_node->full_path_dest = malloc(full_dest_len);
    
    if (new_node->full_path == NULL|| new_node->full_path_dest==NULL)
    {   free(new_node->full_path_dest);
        free(new_node->name);
        free(new_node);
        ERR("malloc");
        return;
    }

    strcpy(new_node->full_path, wd_node->path);
    strcpy(new_node->full_path_dest, wd_node->path_new);
    if (name_len > 0)
    {
        strcat(new_node->full_path, "/");
        strcat(new_node->full_path, event->name);
        strcat(new_node->full_path_dest, "/");
        strcat(new_node->full_path_dest, event->name); 
    }

    //adding the element
    new_node->next = NULL;
    pthread_mutex_lock(&l->mtx);
    new_node->prev = l->tail;

    if (l->tail != NULL)
    {
        l->tail->next = new_node;
    }
    else
    {
        l->head = new_node;
    }

    l->tail = new_node;
    l->size++;
    pthread_mutex_unlock(&l->mtx);
}

void remove_inotify_event()
{
    Ino_List *l = &inotify_events;
    if (l == NULL || l->head == NULL)
    {
        return;
    }

    pthread_mutex_lock(&l->mtx);
    if (l->head == NULL)
    {
        pthread_mutex_unlock(&l->mtx);
        return;
    }
    Ino_Node *removed = l->head;
    l->head = removed->next;
    if (l->head != NULL)
    {
        l->head->prev = NULL;
    }
    else
    {
        l->tail = NULL;
    }
    l->size--;
    pthread_mutex_unlock(&l->mtx);
    free(removed->full_path_dest);
    free(removed->name);
    free(removed->full_path);
    free(removed);
}

/*

    MOVE EVENTS LIST

*/
void delete_node(M_node * current, M_list * l){
    if (current->prev != NULL)
            {
                current->prev->next = current->next;
            }
            else
            {
                l->head = current->next;
            }

            if (current->next != NULL)
            {
                current->next->prev = current->prev;
            }
            else
            {
                l->tail = current->prev;
            }

            free(current->move_from);
            free(current->move_to);
            free(current);
            l->size--;
            

}

char* add_move_from_event(int cookie, char* move_from, char* full_dest_path)
{
    M_list* l = &move_events;
    if (l == NULL || move_from == NULL || full_dest_path == NULL)
    {
        return NULL;
    }

    time_t current_time = time(NULL);
    char* result = NULL;

    pthread_mutex_lock(&l->mtx);
    
    M_node* current = l->head;
    
    while (current != NULL)
    {
        M_node* next = current->next;
        
        // Check if node is older than 10 seconds
        

        // Check if cookie matches
        if (current->cookie == cookie)
        {
            result = strdup(current->move_to);
            
            // Delete matched node
           delete_node(current,l);
            
            
        }
        else if (difftime(current_time, current->token) > 10.0)
        {
            // Placeholder: node expired (only move_from or only move_to)
            #ifdef DEBUG
            fprintf(stderr, "[add_move_from_event] PLACEHOLDER: Move event expired (cookie=%u). Action: TODO_HANDLE_EXPIRED_MOVE_FROM\n", current->cookie);
            #endif
            
            // Delete expired node
           delete_node(current, l);
        }

        current = next;
    }
    if(result!=NULL){
        pthread_mutex_unlock(&l->mtx);
        return result;
    }

    // No matching cookie found, add new node
    M_node* new_node = malloc(sizeof(M_node));
    if (new_node == NULL)
    {
        pthread_mutex_unlock(&l->mtx);
        return NULL;
    }

    new_node->cookie = cookie;
    new_node->move_from = strdup(move_from);
    new_node->move_to = NULL;
    new_node->full_dest_path = strdup(full_dest_path);
    new_node->token = current_time;
    new_node->next = NULL;

    if (new_node->move_from == NULL || new_node->full_dest_path == NULL)
    {
        free(new_node->move_from);
        free(new_node->full_dest_path);
        free(new_node);
        ERR("strdup");
    }

    new_node->prev = l->tail;
    if (l->tail != NULL)
    {
        l->tail->next = new_node;
    }
    else
    {
        l->head = new_node;
    }

    l->tail = new_node;
    l->size++;

    #ifdef DEBUG
    fprintf(stderr, "[add_move_from_event] PLACEHOLDER: Only move_from found (cookie=%u, move_from=%s). Action: TODO_HANDLE_ONLY_MOVE_FROM\n", cookie, move_from);
    #endif

    pthread_mutex_unlock(&l->mtx);
    return NULL;
}

char* add_move_to_event(int cookie, char* move_to, char* full_dest_path)
{
    M_list* l = &move_events;
    if (l == NULL || move_to == NULL || full_dest_path == NULL)
    {
        return NULL;
    }

    time_t current_time = time(NULL);
    char* result = NULL;

    pthread_mutex_lock(&l->mtx);
    
    M_node* current = l->head;
    while (current != NULL)
    {
        M_node* next = current->next;
        
        // Check if node is older than 10 seconds
        if (current->cookie == cookie)
        {
            result = strdup(current->move_from);
            
            delete_node(current, l);
        }
        else if (difftime(current_time, current->token) > 10.0)
        {
            // Placeholder: node expired (only move_from or only move_to)
            #ifdef DEBUG
            fprintf(stderr, "[add_move_to_event] PLACEHOLDER: Move event expired (cookie=%u). Action: TODO_HANDLE_EXPIRED_MOVE_TO\n", current->cookie);
            #endif
            
           
            delete_node(current, l);
        }

        current = next;
    }

    if(result!=NULL){
        pthread_mutex_unlock(&l->mtx);
        return result;
    }

    // creating a new node
    M_node* new_node = malloc(sizeof(M_node));
    if (new_node == NULL)
    {
        pthread_mutex_unlock(&l->mtx);
        return NULL;
    }

    new_node->cookie = cookie;
    new_node->move_from = NULL;
    new_node->move_to = strdup(move_to);
    new_node->full_dest_path = strdup(full_dest_path);
    new_node->token = current_time;
    new_node->next = NULL;

    if (new_node->move_to == NULL || new_node->full_dest_path == NULL)
    {
        free(new_node->move_to);
        free(new_node->full_dest_path);
        free(new_node);
        ERR("strdup");
    }

    //adding at the tail
    new_node->prev = l->tail;
    if (l->tail != NULL)
    {
        l->tail->next = new_node;
    }
    else
    {
        l->head = new_node;
    }

    l->tail = new_node;
    l->size++;

    #ifdef DEBUG
    fprintf(stderr, "[add_move_to_event] PLACEHOLDER: Only move_to found (cookie=%u, move_to=%s). Action: TODO_HANDLE_ONLY_MOVE_TO\n", cookie, move_to);
    #endif

    pthread_mutex_unlock(&l->mtx);
    return NULL;
}

void check_move_events_list()
{
    M_list* l = &move_events;
    if (l == NULL)
    {
        return;
    }

    time_t current_time = time(NULL);

    pthread_mutex_lock(&l->mtx);
    
    M_node* current = l->head;
    while (current != NULL)
    {
        M_node* next = current->next;
        
        if (difftime(current_time, current->token) > 10.0)
        {
            
            if(current->move_from==NULL){
                //just copy the file to the new destination
                //if it is a directory use the goofy ass version
                //if it is a symlink then goofy^2 method
            }
            if(current->move_to==NULL){
                //delete the file
                //if it is a direcotry once again goofy ass delete
            }
            
            // Delete expired node
            delete_node(current, l);
        }

        current = next;
    }
    
    pthread_mutex_unlock(&l->mtx);
}