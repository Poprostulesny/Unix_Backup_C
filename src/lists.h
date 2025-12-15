#ifndef LISTS_H
#define LISTS_H

#include "lists_common.h"
#include "list_targets.h"
#include "list_sources.h"
#include "list_wd.h"
#include "list_init_backup.h"
#include "list_inotify_events.h"
#include "list_move_events.h"

/* GLOBAL VARIABLES*/
extern list_bck init_backup_tasks;
extern list_sc backups;
extern list_wd wd_list;
extern Ino_List inotify_events;
extern M_list move_events;
/*-------------------*/

// Initialize all global lists mutexes and reset their state
void init_lists(void);

#endif /* LISTS_H */