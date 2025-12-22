#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
int is_target_in_source(char* source, char* target);
char* concat(int n, ...);
char* get_end_suffix(const char* base, const char* full);
char* tokenizer(char* tok);
#endif /* UTILS_H */
