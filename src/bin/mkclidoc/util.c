#include "util.h"

#include <stdlib.h>
#include <string.h>

void *xmalloc(size_t size)
{
    void *m = malloc(size);
    if (!m) abort();
    return m;
}

void *xrealloc(void *ptr, size_t size)
{
    void *m = realloc(ptr, size);
    if (!m) abort();
    return m;
}

char *copystr(const char *str)
{
    size_t sz = strlen(str) + 1;
    char *res = xmalloc(sz);
    memcpy(res, str, sz);
    return res;
}

