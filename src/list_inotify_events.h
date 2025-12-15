#ifndef LIST_INOTIFY_EVENTS_H
#define LIST_INOTIFY_EVENTS_H

#include "lists_common.h"
#include <sys/inotify.h>

// Function to add an inotify event to the end of the list
void add_inotify_event(struct inotify_event *event);

// Function to remove the inotify event from the front of the list
void remove_inotify_event(void);

#endif /* LIST_INOTIFY_EVENTS_H */
