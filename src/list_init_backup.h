#ifndef LIST_INIT_BACKUP_H
#define LIST_INIT_BACKUP_H

#include "lists_common.h"

// Function to add a backup job to a queue
void init_backup_add_job(list_bck* list, char* source, char* source_friendly, char* target);

// Pop first backup job from a queue (returns NULL if none)
Node_init* init_backup_pop_job(list_bck* list);

#endif /* LIST_INIT_BACKUP_H */
