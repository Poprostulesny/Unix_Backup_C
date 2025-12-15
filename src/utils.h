#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

int min(int a, int b);
int is_target_in_source(char* source, char* target);
char* concat(int n, ...);

#endif /* UTILS_H */
