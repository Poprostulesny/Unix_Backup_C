#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

//How long in seconds will the values be held in mov buffer
#define MOV_TIME 15.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include<sys/stat.h>
#include <time.h>
#include "generic_file_functions.h"
#include "list_move_events.h"
#include "lists_common.h"
#include "inotify_functions.h"
#include "utils.h"
#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

extern M_list move_events;

void delete_node(M_list* l, M_node * current){
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

static void handle_completed_move(M_list* l, M_node* node, node_sc* source)
{
    if (node == NULL || source == NULL)
    {
        return;
    }

    char* suf_to = get_end_suffix(source->source_full, node->move_to);
    char* suf_from = get_end_suffix(source->source_full, node->move_from);
    for_each_target_path(source, suf_to, move_all, suf_from);
    free(suf_from);
    free(suf_to);
    delete_node(l, node);
}

static void handle_expired_node(M_list* l, M_node* current, node_sc* source)
{
    if (current == NULL || source == NULL)
    {
        delete_node(l, current);
        return;
    }

    if (current->move_from == NULL && current->move_to != NULL)
    {
        char* suf_to = get_end_suffix(source->source_full, current->move_to);
        copy_to_all_targets(current->move_to, suf_to, &source->targets);
        free(suf_to);
    }
    else if (current->move_to == NULL && current->move_from != NULL)
    {
        char* suf_from = get_end_suffix(source->source_full, current->move_from);
        for_each_target_path(source, suf_from, delete_multi, NULL);
        free(suf_from);
    }

    delete_node(l, current);
}

void add_move_event(M_list* l, int cookie, const char* path, int is_from, node_sc* source)
{
    if (l == NULL || path == NULL || source == NULL)
    {
        return;
    }

    time_t current_time = time(NULL);
    int matched = 0;

    pthread_mutex_lock(&l->mtx);
    M_node* current = l->head;
    while (current != NULL)
    {
        M_node* next = current->next;

        if (current->cookie == cookie)
        {
            char** target_field = is_from ? &current->move_from : &current->move_to;
            if (*target_field == NULL)
            {
                *target_field = strdup(path);
                if (*target_field == NULL)
                {
                    pthread_mutex_unlock(&l->mtx);
                    ERR("strdup");
                }
            }
            current->token = current_time;

            if (current->move_from != NULL && current->move_to != NULL)
            {
                handle_completed_move(l, current, source);
            }
            matched = 1;
        }
        else if (difftime(current_time, current->token) > MOV_TIME)
        {
            handle_expired_node(l, current, source);
        }

        current = next;
    }

    if (matched)
    {
        pthread_mutex_unlock(&l->mtx);
        return;
    }

    M_node* new_node = malloc(sizeof(M_node));
    if (new_node == NULL)
    {
        pthread_mutex_unlock(&l->mtx);
        ERR("malloc");
    }

    new_node->cookie = cookie;
    new_node->move_from = is_from ? strdup(path) : NULL;
    new_node->move_to = is_from ? NULL : strdup(path);
    new_node->token = current_time;
    new_node->next = NULL;

    if ((is_from && new_node->move_from == NULL) || (!is_from && new_node->move_to == NULL))
    {
        free(new_node->move_from);
        free(new_node->move_to);
        free(new_node);
        pthread_mutex_unlock(&l->mtx);
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

    pthread_mutex_unlock(&l->mtx);
}

void check_move_events_list(M_list* l, node_sc * source)
{
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
        
        if (difftime(current_time, current->token) > MOV_TIME)
        {
            handle_expired_node(l, current, source);
        }

        current = next;
    }
    
    pthread_mutex_unlock(&l->mtx);
}
