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

extern list_bck init_backup_tasks;

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
