#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef ERR
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#endif
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
    size_t total = 0;
    va_list args;

    va_start(args, n);
    for (int i = 0; i < n; i++)
    {
        const char* s = va_arg(args, const char*);
        if (s)
            total += strlen(s);
    }
    va_end(args);

    char* out = malloc(total + 1);
    if (!out)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    char* p = out;
    va_start(args, n);
    for (int i = 0; i < n; i++)
    {
        const char* s = va_arg(args, const char*);
        if (!s)
            continue;
        size_t len = strlen(s);
        memcpy(p, s, len);
        p += len;
    }
    va_end(args);

    *p = '\0';
    return out;
}

char* get_end_suffix(const char* base,const char* full)
{
    int i = 0;
    while (base[i] == full[i] && i < min(strlen(base), strlen(full)))
    {
        i++;
    }

    if (strlen(base) == strlen(full))
    {
        char* result = strdup("");
        if (result == NULL)
        {
            ERR("strdup");
        }
        return result;
    }

    char* result = strdup((full + i));
    if (result == NULL)
    {
        ERR("strdup");
    }
    return result;
}
