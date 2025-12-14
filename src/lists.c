#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lists.h"


/* GLOBAL VARIABLES*/
list_bck init_backup_tasks;
list_sc backups;
list_wd wd_list;
Ino_List inotify_events;
/*-------------------*/

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

        node_tr* current_target = node->targets.head;
        while (current_target != NULL)
        {
            node_tr* temp = current_target;
            current_target = current_target->next;
            delete_target_node(temp);
        }
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
}

// Function to delete a target node by full target name
void list_target_delete(list_tg* l, char* target)
{
    if (l == NULL || target == NULL)
    {
        return;
    }

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
            printf("Deleted node with full path: %s\n", current->target_full);
            l->size--;
            return;
        }
        current = current->next;
    }
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
    fprintf(stderr, "ADD node_sc %p (source=%s)\n", (void*)new_node, new_node->source_friendly);

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
}

// Function to delete a source node by source name
void list_source_delete(char* source)
{
    list_sc* l = &backups;
    if (l == NULL || source == NULL)
    {
        return;
    }

    node_sc* current = l->head;
    fprintf(stderr, "DEL node_sc %p (source=%s)\n", (void*)current, current->source_friendly);

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

            delete_source_node(current);
            l->size--;
            return;
        }
        current = current->next;
    }
}

// Function to check whether an element is already a target by the friendly name
int find_element_by_target_help(list_tg* l, char* target)
{
    if (l == NULL || target == NULL)
    {
        return 0;
    }

    node_tr* current = l->head;
    while (current != NULL)
    {
        if (strcmp(current->target_friendly, target) == 0)
        {
            return 1;
        }
        current = current->next;
    }
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
    node_sc* current = l->head;
    while (current != NULL)
    {
        if (find_element_by_target_help(&current->targets, target) == 1)
        {
            return 1;
        }
        current = current->next;
    }
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

    node_sc* current = l->head;
    while (current != NULL)
    {
        if (current->source_friendly == NULL)
        {
            fprintf(stderr, "BUG: current=%p, current->source == NULL (list corrupted)\n", (void*)current);
            abort();
        }
        if (strcmp(current->source_friendly, source) == 0)
        {
            return current;
        }
        current = current->next;
    }
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
            return;
        }
        current = current->next;
    }
}

void add_wd_node(int wd, char* source_friendly, char* source_full, char* path)
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
    new_node->next = NULL;
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
}

Node_wd* find_element_by_wd(int wd)
{
    list_wd* l = &wd_list;
    if (l == NULL)
    {
        return NULL;
    }

    Node_wd* current = l->head;
    while (current != NULL)
    {
        if (current->wd == wd)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}


void delete_all_wd_by_path(char * source_friendly){
    list_wd* l = &wd_list;
    if (l == NULL)
    {
        return;
    }

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
            free(current);
            l->size--;

        }
        current = next;
    }
    

}

// Functions for init backup tasks
void init_backup_add_job(char* source, char* target)
{
    list_bck* l = &init_backup_tasks;
    if (l == NULL || source == NULL || target == NULL)
    {
        return;
    }

    Node_init* new_node = malloc(sizeof(Node_init));
    if (new_node == NULL)
    {
        return;
    }

    new_node->source_full = strdup(source);
    new_node->target_full = strdup(target);
    new_node->next = NULL;
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
}

void init_backup_job_done()
{
    list_bck* l = &init_backup_tasks;
    if (l == NULL || l->head == NULL)
    {
        return;
    }

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

    free(to_delete->source_full);
    free(to_delete->target_full);
    free(to_delete);
    l->size--;
}

// Function to get source_friendly by source_full
char* get_source_friendly(char* source_full)
{
    list_sc* l = &backups;
    if (l == NULL || source_full == NULL)
    {
        return NULL;
    }

    node_sc* current = l->head;
    while (current != NULL)
    {
        if (strcmp(current->source_full, source_full) == 0)
        {
            return strdup(current->source_friendly);
        }
        current = current->next;
    }
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

    if(event->name!=NULL){
        new_node->name = strdup(event->name);
        if(new_node->name==NULL){
            ERR("strdup");
        }
    }
    else{
        new_node->name=NULL;
    }
    

    //creating full path to the element
    size_t path_len = strlen(wd_node->path);
    size_t name_len;
    if (event->len > 0) {
        name_len = strlen(event->name);
    } else {
        name_len = 0;
    }

    size_t full_len = path_len;
    if (name_len > 0) {
        full_len += 1 + name_len;
    }
    full_len += 1; //adding '/'
    new_node->full_path = malloc(full_len);

    if (new_node->full_path == NULL)
    {   
        free(new_node->name);
        free(new_node);
        ERR("malloc");
        return;
    }

    strcpy(new_node->full_path, wd_node->path);
    if (name_len > 0)
    {
        strcat(new_node->full_path, "/");
        strcat(new_node->full_path, event->name);
    }

    //adding the element
    new_node->next = NULL;
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
}

void remove_inotify_event()
{
    Ino_List *l = &inotify_events;
    if (l == NULL || l->head == NULL)
    {
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

    free(removed->name);
    free(removed->full_path);
    free(removed);
}