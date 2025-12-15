#ifndef LIST_INIT_BACKUP_H
#define LIST_INIT_BACKUP_H

#include "lists_common.h"

typedef struct Node_init
{
    struct Node_init* next;
    struct Node_init* prev;
    char* source_full;
    char* target_full;
    char* source_friendly;
} Node_init;

typedef struct Init_backup_task_list
{
    pthread_mutex_t mtx;
    struct Node_init* head;
    struct Node_init* tail;
    int size;
} list_bck;

// Function to add a backup job to a queue
void init_backup_add_job(char* source, char* source_friendly, char* target);

// Function to mark the backup job as done
void init_backup_job_done();

#endif /* LIST_INIT_BACKUP_H */
