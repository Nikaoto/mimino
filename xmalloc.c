#include <stdio.h>

void*
xmalloc(size_t size)
{
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "malloc() failed, aborting!");
        exit(1);
    }
    return p;
}

void*
xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (!p) {
        fprintf(stderr, "realloc() failed, aborting!");
        exit(1);
    }
    return p;
}
