#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "utils.h"

int min(int a, int b) { return a < b ? a : b; }

// returns 1 if target is in source, 0 otherwise
int is_target_in_source(char* source, char* target)
{
    int i = 0;
    int n = min((int)strlen(target), (int)strlen(source));
    while (source[i] == target[i] && i < n)
    {
        i++;
    }
    if (i == (int)strlen(source))
    {
        return 1;
    }

    return 0;
}

char* concat(int n, ...)
{
    int total_lenght = 0;
    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++)
    {
        total_lenght += strlen(va_arg(args, char*));
    }
    va_end(args);

    char* re = malloc(total_lenght + 1);
    if (!re)
        perror("malloc"), exit(EXIT_FAILURE);
    re[0] = 0;

    va_start(args, n);
    for (int i = 0; i < n; i++)
    {
        strcat(re, va_arg(args, char*));
    }
    va_end(args);
    return re;
}
