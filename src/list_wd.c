#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "list_wd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif

// Functions for wd list
void delete_wd_node(list_wd* l, int wd)
{
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
            free(current->suffix);
            free(current);
            l->size--;
            return;
        }
        current = current->next;
    }
}

void add_wd_node(list_wd* l, int wd, char* source_friendly, char* source_full, const char* path, const char* suffix)
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
    if (suffix != NULL)
    {
        new_node->suffix = strdup(suffix);
    }
    else
    {
        new_node->suffix = strdup("");
    }
    new_node->next = NULL;
    if (new_node->source_friendly == NULL || new_node->source_full == NULL || new_node->path == NULL ||
        new_node->suffix == NULL)
    {
        free(new_node->source_friendly);
        free(new_node->source_full);
        free(new_node->path);
        free(new_node->suffix);
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
}

Node_wd* find_element_by_wd(list_wd* l, int wd)
{
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
