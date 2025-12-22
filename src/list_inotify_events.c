#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "list_inotify_events.h"
#include "list_wd.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>

#ifndef ERR
#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   exit(EXIT_FAILURE))
#endif

void add_inotify_event(list_wd *wd_list, Ino_List *l,
                       struct inotify_event *event) {
  if (l == NULL || event == NULL) {
    return;
  }

  Node_wd *wd_node = find_element_by_wd(wd_list, event->wd);
  if (wd_node == NULL) {
    return;
  }

  Ino_Node *new_node = malloc(sizeof(Ino_Node));
  if (new_node == NULL) {
    ERR("malloc");
  }

  new_node->wd = event->wd;
  new_node->mask = event->mask;
  new_node->cookie = event->cookie;
  new_node->len = event->len;

  new_node->name = strdup(event->name);
  if (new_node->name == NULL) {
    ERR("strdup");
  }

  // creating full path to the element
  size_t path_len = strlen(wd_node->path);
  size_t name_len;
  if (event->len > 0) {
    name_len = strlen(event->name);
  } else {
    name_len = 0;
  }

  size_t full_len = path_len;
  if (name_len > 0) {
    full_len += 1 + name_len;
  }
  full_len += 1; // adding '/'
  new_node->full_path = malloc(full_len);

  if (new_node->full_path == NULL) {
    free(new_node->name);
    free(new_node);
    ERR("malloc");
    return;
  }

  strcpy(new_node->full_path, wd_node->path);
  if (name_len > 0) {
    strcat(new_node->full_path, "/");
    strcat(new_node->full_path, event->name);
  }

  new_node->suffix = get_end_suffix(wd_node->source_full, new_node->full_path);
  if (new_node->suffix == NULL) {
    new_node->suffix = strdup("");
  }
  if (new_node->suffix == NULL) {
    free(new_node->full_path);
    free(new_node->name);
    free(new_node);
    ERR("strdup");
  }

  // adding the element
  new_node->next = NULL;
  new_node->prev = l->tail;

  if (l->tail != NULL) {
    l->tail->next = new_node;
  } else {
    l->head = new_node;
  }

  l->tail = new_node;
  l->size++;
}

void remove_inotify_event(Ino_List *l) {
  if (l == NULL || l->head == NULL) {
    return;
  }

  if (l->head == NULL) {
    return;
  }
  Ino_Node *removed = l->head;
  l->head = removed->next;
  if (l->head != NULL) {
    l->head->prev = NULL;
  } else {
    l->tail = NULL;
  }
  l->size--;
  free(removed->name);
  free(removed->suffix);
  free(removed->full_path);
  free(removed);
}
