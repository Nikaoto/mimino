#ifndef _MIMINO_BUFFER_H
#define _MIMINO_BUFFER_H

#include <stdio.h>
#include <stdarg.h>
#include "dir.h"

#define BUFFER_GROWTH 4096

typedef struct {
    char *data;
    size_t n_items; // Number items in data
    size_t n_alloc; // Number of bytes allocated for data
} Buffer;

Buffer* new_buf(size_t data_size);
void free_buf(Buffer*);
void free_buf_parts(Buffer*);

void buf_grow(Buffer *b, size_t min_growth);
void buf_push(Buffer *b, char c);
void buf_append(Buffer *b, char *src, size_t n);
void buf_append_str(Buffer *b, char *str);
int buf_sprintf(Buffer *buf, char *fmt, ...);
void buf_append_href(Buffer *buf, File *f, char *req_path);
int buf_append_file_contents(Buffer *buf, File *f, char *path);

#endif
