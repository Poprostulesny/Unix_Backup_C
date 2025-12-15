#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "lists.h"


/* GLOBAL VARIABLES*/
list_bck init_backup_tasks;
list_sc backups;
list_wd wd_list;
Ino_List inotify_events;
M_list move_events;
/*-------------------*/

void init_lists(void)
{
    // Backups list
    backups.head = NULL;
    backups.tail = NULL;
    backups.size = 0;
    if (pthread_mutex_init(&backups.mtx, NULL) != 0)
    {
        ERR("pthread_mutex_init backups");
    }

    // Watch descriptors list
    wd_list.head = NULL;
    wd_list.tail = NULL;
    wd_list.size = 0;
    if (pthread_mutex_init(&wd_list.mtx, NULL) != 0)
    {
        ERR("pthread_mutex_init wd_list");
    }

    // Initial backup tasks queue
    init_backup_tasks.head = NULL;
    init_backup_tasks.tail = NULL;
    init_backup_tasks.size = 0;
    if (pthread_mutex_init(&init_backup_tasks.mtx, NULL) != 0)
    {
        ERR("pthread_mutex_init init_backup_tasks");
    }

    // Inotify events queue
    inotify_events.head = NULL;
    inotify_events.tail = NULL;
    inotify_events.size = 0;
    if (pthread_mutex_init(&inotify_events.mtx, NULL) != 0)
    {
        ERR("pthread_mutex_init inotify_events");
    }
    
    //Move events dict
    move_events.head=NULL;
    move_events.tail=NULL;
    move_events.size=0;
    if(pthread_mutex_init(&move_events.mtx, NULL)!=0){  
        ERR("pthread_mutex_init move_events");

    }
}