#ifndef _MIMINO_XMALLOC_H
#define _MIMINO_XMALLOC_H

#include <stdio.h>

void* xmalloc(size_t size);
void* xrealloc(void *ptr, size_t size);

#endif
