#include <stdlib.h>
#include <string.h>
#include "ascii.h"
#include "buffer.h"
#include "xmalloc.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// Allocates new Buffer and its data
Buffer*
new_buf(size_t data_size)
{
    Buffer *b = xmalloc(sizeof(Buffer));
    b->data = xmalloc(data_size);
    b->n_alloc = data_size;
    b->n_items = 0;
    return b;
}

// Only allocates the data of the Buffer
Buffer*
init_buf(Buffer *b, size_t data_size)
{
    b->data = xmalloc(data_size);
    b->n_alloc = data_size;
    b->n_items = 0;
    return b;
}

void
free_buf(Buffer *b)
{
    if (!b) return;
    if (b->data) free(b->data);
    free(b);
}

// Free all parts of a Buffer
void
free_buf_parts(Buffer *b)
{
    free(b->data);
}

void
buf_grow(Buffer *b, size_t min_growth)
{
    size_t new_size = b->n_alloc + MAX(min_growth, BUFFER_GROWTH);
    char *ptr = xrealloc(b->data, new_size);
    b->n_alloc = new_size;
    b->data = ptr;
}

// Push one byte into buffer's data, growing it if necessary.
// Return 0 on fail.
// Return 1 on success.
void
buf_push(Buffer *b, char c)
{
    if (b->n_items + 1 > b->n_alloc) {
        buf_grow(b, 1);
    }
    b->data[b->n_items] = c;
    b->n_items++;
}

// Append n bytes from src to buffer's data, growing it if necessary.
void
buf_append(Buffer *b, char *src, size_t n)
{
    if (n == 0) return;

    if (b->n_items + n > b->n_alloc) {
        buf_grow(b, n);
    }

    memcpy(b->data + b->n_items, src, n);
    b->n_items += n;
}

// Does not copy the null terminator
void
buf_append_str(Buffer *b, char *str)
{
    buf_append(b, str, strlen(str));
}

// Does not copy the null terminator
void
buf_append_buf(Buffer *dest, Buffer *src)
{
    if (src->n_items == 0) return;

    if (dest->n_items + src->n_items > dest->n_alloc) {
        buf_grow(dest, src->n_items);
    }

    memcpy(dest->data + dest->n_items, src->data, src->n_items);
    dest->n_items += src->n_items;
}

// Does not copy the null terminator
int
buf_sprintf(Buffer *buf, char *fmt, ...)
{
    va_list fmtargs;
    int len;

    // Determine formatted length
    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);
    len++;

    // Grow buffer if necessary
    if (buf->n_items + len > buf->n_alloc)
        buf_grow(buf, len);

    va_start(fmtargs, fmt);
    vsnprintf(buf->data + buf->n_items, len, fmt, fmtargs);
    va_end(fmtargs);

    // Exclude the null terminator at the end
    buf->n_items += len - 1;

    return len - 1;
}

// Return -1 on fopen error
// Return 0 on read error
// Return 1 on success
int
buf_append_file_contents(Buffer *buf, File *f, char *path)
{
    if (f->size == 0)
        return 1;

    if (buf->n_items + f->size > buf->n_alloc)
        buf_grow(buf, f->size);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen()");
        return -1;
    }

    while (1) {
        size_t bytes_read = fread(buf->data + buf->n_items, 1, f->size, fp);
        buf->n_items += bytes_read;
        if (bytes_read < (size_t) f->size) {
            if (ferror(fp)) {
                fprintf(stdout, "Error when freading() file %s\n", path);
                fclose(fp);
                return 0;
            }
            // EOF
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 1;
}

void
print_buf_ascii(FILE *stream, Buffer *buf)
{
    if (buf->n_items == 0) {
        fprintf(stream, "(No data do print)\n");
        return;
    }

    for (size_t i = 0; i < buf->n_items; i++) {
        switch (buf->data[i]) {
        case '\n':
            fprintf(stream, "\\n\n");
            break;
        case '\t':
            fprintf(stream, "\\t");
            break;
        case '\r':
            fprintf(stream, "\\r");
            break;
        default:
            putc(buf->data[i], stream);
            break;
        }
    }
}
