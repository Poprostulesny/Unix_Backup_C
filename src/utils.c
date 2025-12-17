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

// returns 1 if target is in source, 0 otherwise
int is_target_in_source(char* source, char* target)
{
    if (source == NULL || target == NULL)
    {
        return 0;
    }

    size_t source_len = strlen(source);
    size_t target_len = strlen(target);

    if (source_len == 0 || target_len < source_len)
    {
        return 0;
    }

    if (strncmp(source, target, source_len) != 0)
    {
        return 0;
    }

    if (target_len == source_len)
    {
        return 1;
    }

    return target[source_len] == '/';
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

char* get_end_suffix(const char* base, const char* full)
{
    if (base == NULL || full == NULL)
    {
        return NULL;
    }

    size_t base_len = strlen(base);
    size_t full_len = strlen(full);
    size_t limit = (base_len < full_len) ? base_len : full_len;
    size_t i = 0;

    while (i < limit && base[i] == full[i])
    {
        i++;
    }

    if (base_len == full_len && i == base_len)
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
