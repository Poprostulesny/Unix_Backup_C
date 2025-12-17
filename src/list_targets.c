#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list_targets.h"

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
