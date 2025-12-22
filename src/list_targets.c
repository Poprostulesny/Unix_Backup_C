#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "list_targets.h"
#include "list_inotify_events.h"
#include "list_move_events.h"
#include "list_wd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Helper function to delete a single target node
void delete_target_node(node_tr *node) {
  if (node != NULL) {
    if (node->inotify_fd >= 0) {
      close(node->inotify_fd);
    }

    // free watcher list
    Node_wd *wd_cur = node->watchers.head;
    while (wd_cur != NULL) {
      Node_wd *next = wd_cur->next;
      free(wd_cur->source_friendly);
      free(wd_cur->source_full);
      free(wd_cur->path);
      free(wd_cur->suffix);
      free(wd_cur);
      wd_cur = next;
    }

    // free inotify events list
    Ino_Node *ino_cur = node->events.head;
    while (ino_cur != NULL) {
      Ino_Node *next = ino_cur->next;
      free(ino_cur->name);
      free(ino_cur->full_path);
      free(ino_cur->suffix);
      free(ino_cur);
      ino_cur = next;
    }

    // free move dict
    M_node *mcur = node->mov_dict.head;
    while (mcur != NULL) {
      M_node *next = mcur->next;
      free(mcur->move_from);
      free(mcur->move_to);
      free(mcur);
      mcur = next;
    }

    free(node->target_friendly);
    free(node->target_full);
    free(node);
  }
}

// Function to add a target node at the beginning of target list
void list_target_add(list_tg *l, node_tr *new_node) {
  if (l == NULL || new_node == NULL) {
    return;
  }

  new_node->inotify_fd = -1;
  new_node->child_pid = -1;
  new_node->watchers.head = NULL;
  new_node->watchers.tail = NULL;
  new_node->watchers.size = 0;
  new_node->events.head = NULL;
  new_node->events.tail = NULL;
  new_node->events.size = 0;
  new_node->mov_dict.head = NULL;
  new_node->mov_dict.tail = NULL;
  new_node->mov_dict.size = 0;

  new_node->next = l->head;
  new_node->previous = NULL;

  if (l->head != NULL) {
    l->head->previous = new_node;
  } else {
    l->tail = new_node;
  }

  l->head = new_node;
  l->size++;
}

// Function to delete a target node by full target name; returns 1 on delete, 0
// if not found
int list_target_delete(list_tg *l, char *target) {
  if (l == NULL || target == NULL) {
    return 0;
  }

  node_tr *current = l->head;
  while (current != NULL) {
    if (strcmp(current->target_friendly, target) == 0) {
      // Found the node to delete
      if (current->previous != NULL) {
        current->previous->next = current->next;
      } else {
        l->head = current->next;
      }

      if (current->next != NULL) {
        current->next->previous = current->previous;
      } else {
        l->tail = current->previous;
      }

      delete_target_node(current);
#ifdef DEBUG
      printf("Deleted node with full path: %s\n", current->target_full);
#endif

      l->size--;
      return 1;
    }
    current = current->next;
  }
  printf("Target not found: %s\n", target);
  return 0;
}

// Function to check whether an element is already a target by the friendly name
int find_element_by_target_help(list_tg *l, char *target) {
  if (l == NULL || target == NULL) {
    return 0;
  }

  node_tr *current = l->head;
  while (current != NULL) {
    if (strcmp(current->target_friendly, target) == 0 ||
        is_target_in_source(current->target_friendly, target) == 1) {
      return 1;
    }
    current = current->next;
  }
  return 0;
}
