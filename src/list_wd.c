#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "list_wd.h"

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

extern list_wd wd_list;

// Functions for wd list
void delete_wd_node(list_wd* l, int wd)
{
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
                free(current->path_new);
                free(current->full_target_path);
                free(current);
            l->size--;
            pthread_mutex_unlock(&l->mtx);
            return;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&l->mtx);
}

void add_wd_node(list_wd* l, int wd, char* source_friendly, char* source_full, char* path_new, const char* path, const char* full_target_path)
{
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
    new_node->path_new = strdup(path_new);
    new_node->full_target_path = strdup(full_target_path);
    new_node->next = NULL;
    if (new_node->source_friendly == NULL || new_node->source_full == NULL || new_node->path == NULL || new_node->path_new == NULL || new_node->full_target_path == NULL)
    {
        free(new_node->source_friendly);
        free(new_node->source_full);
        free(new_node->path);
        free(new_node->path_new);
        free(new_node->full_target_path);
        free(new_node);
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

Node_wd* find_element_by_wd(list_wd* l, int wd)
{
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


void delete_all_wd_by_path(list_wd* l, char * source_friendly){
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
            free(current->full_target_path);
            free(current);
            l->size--;

        }
        current = next;
    }
    pthread_mutex_unlock(&l->mtx);
    

}
