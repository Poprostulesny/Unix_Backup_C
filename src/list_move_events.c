#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "list_move_events.h"

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

extern M_list move_events;

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
