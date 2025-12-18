#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list_init_backup.h"

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif


// Functions for init backup tasks
void init_backup_add_job(list_bck* l, char* source, char* source_friendly, char* target)
{
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

//Pop from the queue of the init lists
Node_init* init_backup_pop_job(list_bck* l)
{
    if (l == NULL)
    {
        return NULL;
    }

    pthread_mutex_lock(&l->mtx);
    if (l->head == NULL)
    {
        pthread_mutex_unlock(&l->mtx);
        return NULL;
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

    l->size--;
    pthread_mutex_unlock(&l->mtx);
    return to_delete;
}
