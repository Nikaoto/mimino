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

Buffer*
new_buf(size_t data_size)
{
    Buffer *b = xmalloc(sizeof(Buffer));
    b->data = xmalloc(data_size);
    b->n_alloc = data_size;
    b->n_items = 0;
    return b;
}

void
free_buf(Buffer *b)
{
    free(b->data);
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
    return buf_append(b, str, strlen(str));
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

// Encode url and copy it into buf
void
buf_encode_url(Buffer *buf, char *url)
{
    char hex[] = "0123456789ABCDEF";

    for (; *url != '\0'; url++) {
        if (needs_encoding((unsigned char)*url)) {
            buf_push(buf, '%');
            buf_push(buf, hex[(*url >> 4) & 0xF]);
            buf_push(buf, hex[ *url       & 0xF]);
        } else {
            buf_push(buf, *url);
        }
    }
}

void
buf_append_href(Buffer *buf, File *f, char *req_path)
{
    buf_encode_url(buf, f->name);

    // trailing '/' if given file is a directory
    if (f->is_dir) buf_push(buf, '/');
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
