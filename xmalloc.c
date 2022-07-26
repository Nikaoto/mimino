#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void*
xmalloc(size_t size)
{
    void *p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "malloc() failed, aborting!\n");
        exit(1);
    }
    return p;
}

void*
xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "realloc() failed, aborting!\n");
        exit(1);
    }
    return p;
}

char*
xstrdup(char *src)
{
    char *p = strdup(src);
    if (p == NULL) {
        fprintf(stderr, "strdup() failed, aborting!\n");
        exit(1);
    }
    return p;
}

char*
xstrndup(char *src, size_t n)
{
    char *p = strndup(src, n);
    if (p == NULL) {
        fprintf(stderr, "strndup() failed, aborting!\n");
        exit(1);
    }
    return p;
}
