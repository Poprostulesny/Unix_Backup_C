#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define DEBUG
#include "file_utils.h"

/*
    GLOBAL VARIABLES
*/
char* _source;
char* _source_friendly;
char* _target;
int fd;

