#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lists.h"

/* GLOBAL VARIABLES*/
list_sc backups;
int inotify_fd;
char * _source;
char * _target;
list_wd wd_list;
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
        return NULL;
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