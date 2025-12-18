#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "list_sources.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "list_inotify_events.h"
#include "list_move_events.h"
#include "list_targets.h"
#include "list_wd.h"

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

extern list_sc backups;

// Helper function to delete a single source node
void delete_source_node(node_sc* node)
{
    if (node != NULL)
    {
        // Free memory of the source node, first the generic fields
        free(node->source_friendly);
        free(node->source_full);

        // Free memory of the targets list
        pthread_mutex_lock(&node->targets.mtx);
        if (node->targets.size > 0)
        {
            node_tr* current_target = node->targets.head;
            while (current_target != NULL)
            {
                node_tr* temp = current_target;
                current_target = current_target->next;
                delete_target_node(temp);
            }
        }
        pthread_mutex_unlock(&node->targets.mtx);
        
        //Free memory of the watch descriptor list
        pthread_mutex_lock(&node->watchers.mtx);
        if (node->watchers.size > 0)
        {
            Node_wd* current = node->watchers.head;
            Node_wd * next;
            while (current != NULL)
            {   
                //errors dont bother us since either way it is ok
                next=current->next;
                inotify_rm_watch(node->fd, current->wd);
                free(current->source_friendly);
                free(current->source_full);
                free(current->path);
                free(current->suffix);
                free(current);
                current = next;;
            }
        }
        pthread_mutex_unlock(&node->watchers.mtx);
        
        //closing the file descriptor
        close(node->fd);

        //free the memory of the move dictionary
        pthread_mutex_lock(&node->mov_dict.mtx);
        if (node->mov_dict.size > 0)
        {
            M_node* current = node->mov_dict.head;
            M_node*next;
            while (current != NULL)
            {   next= current->next;
                free(current->move_from);   
                free(current->move_to);
                free(current);
                current = next;
            }
        }
        pthread_mutex_unlock(&node->mov_dict.mtx);

        // free the memory of the init jobs list
        pthread_mutex_lock(&node->init_jobs.mtx);
        Node_init* job = node->init_jobs.head;
        while (job != NULL)
        {
            Node_init* next_job = job->next;
            free(job->source_full);
            free(job->source_friendly);
            free(job->target_full);
            free(job);
            job = next_job;
        }
        pthread_mutex_unlock(&node->init_jobs.mtx);
        
        //If the lists where instantiated. then destroy the mutexes
        if (node->watchers.size != -1)
        {
            pthread_mutex_destroy(&node->watchers.mtx);
        }
        if (node->events.size != -1)
        {
            pthread_mutex_destroy(&node->events.mtx);
        }
        if (node->mov_dict.size != -1)
        {
            pthread_mutex_destroy(&node->mov_dict.mtx);
        }
        pthread_mutex_destroy(&node->init_jobs.mtx);
        pthread_mutex_destroy(&node->stop_mtx);
        free(node);
    }
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


    if (new_node->targets.head == NULL && new_node->targets.tail == NULL && new_node->targets.size == 0)
    {
        if (pthread_mutex_init(&new_node->targets.mtx, NULL) != 0)
        {
            ERR("pthread_mutex_init targets");
        }
    }
    //Initializing all the lists inside this node
    if (new_node->watchers.size == -1)
    {
        new_node->watchers.head = NULL;
        new_node->watchers.tail = NULL;
        new_node->watchers.size = 0;
        if (pthread_mutex_init(&new_node->watchers.mtx, NULL) != 0)
        {
            ERR("pthread_mutex_init watchers");
        }
    }

    if (new_node->events.size == -1)
    {
        new_node->events.head = NULL;
        new_node->events.tail = NULL;
        new_node->events.size = 0;
        if (pthread_mutex_init(&new_node->events.mtx, NULL) != 0)
        {
            ERR("pthread_mutex_init events");
        }
    }

    if (new_node->mov_dict.size == -1)
    {
        new_node->mov_dict.head = NULL;
        new_node->mov_dict.tail = NULL;
        new_node->mov_dict.size = 0;
        if (pthread_mutex_init(&new_node->mov_dict.mtx, NULL) != 0)
        {
            ERR("pthread_mutex_init move_events");
        }
    }

    pthread_mutex_lock(&l->mtx);
    new_node->next = l->head;
    new_node->previous = NULL;
    new_node->is_inotify_initialized = 0;
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

// Checking whether a given target is already a target by the friendly name
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
        //This functions checks the target lists itself
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
