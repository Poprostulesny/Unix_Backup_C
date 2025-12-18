#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lists_common.h"

/* GLOBAL VARIABLES*/
list_sc backups;
/*-------------------*/

void init_lists(void)
{
    // Backups list
    backups.head = NULL;
    backups.tail = NULL;
    backups.size = 0;
}
