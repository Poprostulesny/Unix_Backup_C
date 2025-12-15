#ifndef LIST_INIT_BACKUP_H
#define LIST_INIT_BACKUP_H

#include "lists_common.h"

// Function to add a backup job to a queue
void init_backup_add_job(char* source, char* source_friendly, char* target);

// Function to mark the backup job as done
void init_backup_job_done(void);

#endif /* LIST_INIT_BACKUP_H */
