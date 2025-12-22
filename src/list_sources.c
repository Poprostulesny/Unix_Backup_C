// clang-format off
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "list_sources.h"
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

extern list_sc backups;

// Helper function to delete a single source node
void delete_source_node(node_sc *node)
{
    if (node != NULL)
    {
        // Free memory of the source node, first the generic fields
        free(node->source_friendly);
        free(node->source_full);

        // Free memory of the targets list
        if (node->targets.size > 0)
        {
            node_tr *current_target = node->targets.head;
            while (current_target != NULL)
            {
                node_tr *temp = current_target;
                current_target = current_target->next;
                delete_target_node(temp);
            }
        }
        free(node);
    }
}

// Function to add a source node at the beginning of source list
void list_source_add(node_sc *new_node)
{
    list_sc *l = &backups;
    if (l == NULL || new_node == NULL)
    {
        return;
    }
#ifdef DEBUG
    fprintf(stderr, "ADD node_sc %p (source=%s)\n", (void *)new_node, new_node->source_friendly);
#endif

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
void list_source_delete(char *source)
{
    list_sc *l = &backups;
    if (l == NULL || source == NULL)
    {
        return;
    }

    node_sc *current = l->head;
#ifdef DEBUG
    fprintf(stderr, "DEL node_sc %p (source=%s)\n", (void *)current, current->source_friendly);
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
            delete_source_node(current);
            return;
        }
        current = current->next;
    }
}

// Checking whether a given target is already a target by the friendly name
int find_element_by_target(char *target)
{
    list_sc *l = &backups;

    if (l == NULL || target == NULL)
    {
        return 0;
    }
    node_sc *current = l->head;
    while (current != NULL)
    {
        // This functions checks the target lists itself
        if (find_element_by_target_help(&current->targets, target) == 1)
        {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// Function to find element by friendly source name
node_sc *find_element_by_source(char *source)
{
    list_sc *l = &backups;
    if (l == NULL || source == NULL)
    {
        return NULL;
    }

    node_sc *current = l->head;
    while (current != NULL)
    {
        if (current->source_friendly == NULL)
        {
#ifdef DEBUG
            fprintf(stderr, "BUG: current=%p, current->source == NULL (list corrupted)\n", (void *)current);
#endif

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

// Function to get source_friendly by source_full
char *get_source_friendly(char *source_full)
{
    list_sc *l = &backups;
    if (l == NULL || source_full == NULL)
    {
        return NULL;
    }

    node_sc *current = l->head;
    while (current != NULL)
    {
        if (strcmp(current->source_full, source_full) == 0)
        {
            char *ret = strdup(current->source_friendly);
            return ret;
        }
        current = current->next;
    }
    return NULL;
}
// clang-format on
