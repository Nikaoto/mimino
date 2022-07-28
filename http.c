#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "http.h"
#include "dir.h"
#include "ascii.h"
#include "xmalloc.h"
#include "buffer.h"
#include "defer.h"

#define DATE_LEN 30
char*
to_rfc1123_date(char *dest, time_t tm) {
    time_t t = tm;
    if (strftime(dest, DATE_LEN,
                 "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t)) == 0) {
        fprintf(stderr, "strftime() failed, aborting!\n");
        exit(1);
    }
    return dest;
}

int
is_valid_http_path_char(char c)
{
    if (is_alpha(c) || is_digit(c))
        return 1;

    return strchr("!$?&'()*+,;=%-._~:@/", (int) c) != NULL;
}

int
can_handle_http_ver(char *ver)
{
    return !strcmp("0.9", ver) || !strcmp("1.0", ver) || !strcmp("1.1", ver);
}

// Return 1 if buf has \r\n\r\n at the end
// Return 0 otherwise
int
is_http_end(char *buf, size_t size)
{
    size_t i = size - 1; // Last char index

    // TODO: check for "\n\n" as well

    if (size < 4) return 0;

    // Ignore trailing null
    if (buf[i] == '\0') {
        if (size < 5) return 0;
        i--;
    }

    return
        buf[i-3] == '\r' && buf[i-2] == '\n' &&
        buf[i-1] == '\r' && buf[i] == '\n';
}

// Return a ^ b.
// Expects b >= 0.
long long
ll_power(long long a, long long b)
{
    if (a == 0) return 0;
    if (a == 1) return 1;
    if (b == 0) return 1;

    long long ret = a;
    while (b > 1) {
        ret *= a;
        b--;
    }

    return ret;
}

// Parses next number, returns its value and advances 'str' to stand on the
// char after number.
// Assumes 'str' starts with a digit.
// 'end' is he outer bound of the string.
long long
consume_next_num(char **str, char *end)
{
    long long num = 0;
    int num_len = 1;
    while (is_digit(*(*str + num_len)) && (*str + num_len) <= end) {
        num_len++;
    }

    for (int i = 0; i < num_len; i++) {
        long long n = *(*str + i) - '0';
        long long increment = ((long long) n) * ll_power(10, num_len - i - 1);
        num += increment;
    }

    *str += num_len;

    return num;
}

int
parse_range_header(
    char *str,
    size_t len,
    int* range_start_given,
    off_t* range_start,
    int* range_end_given,
    off_t* range_end)
{
    char *end = str + len;

    // Advance str by n bytes; Return if running over buffer
    #define SAFE_ADVANCE(str, n) {  \
        if (((str) + (n)) >= end) { \
            return 0;               \
        }                           \
        (str) += (n);               \
    }

    if (strncmp(str, "bytes=", 6)) {
        return 0;
    }
    SAFE_ADVANCE(str, 6);

    // Range start
    if (is_digit(*str)) {
        *range_start_given = 1;
        *range_start = (off_t) consume_next_num(&str, end);
    }

    // The dash between the ranges
    if (*str != '-') {
        *range_start_given = 0;
        return 0;
    }
    SAFE_ADVANCE(str, 1);

    // Range end
    if (is_digit(*str)) {
        *range_end_given = 1;
        *range_end = (off_t) consume_next_num(&str, end);
    }

    // Neither of the ranges were given
    if (!range_start_given && !range_end_given) {
        *range_start = 0;
        *range_end = 0;
        return 0;
    }

    return 1;
    #undef SAFE_ADVANCE
}

// Parses buffer and fills out Http_Request fields
Http_Request*
parse_http_request(Http_Request *req)
{
    // Last char
    char *end = req->buf->data + req->buf->n_items;

    // Advance str by n bytes; Return if running over buffer
    #define SAFE_ADVANCE(str, n) {         \
        if (((str) + (n)) >= end) {        \
            req->error = "Unexpected EOF"; \
            return req;                    \
        }                                  \
        (str) += (n);                      \
    }

    // Left (l) and right (r) boundaries of a token
    char *l = req->buf->data;
    char *r = req->buf->data;

    // Parse method
    while (is_upper_ascii(*r))
        SAFE_ADVANCE(r, 1);
    if (l == r) {
        req->error = "Invalid HTTP method";
        return req;
    }
    req->method = xstrndup(l, (size_t) (r - l));

    // Space
    if (*r != ' ') {
        req->error = "No space after HTTP method";
        return req;
    }
    SAFE_ADVANCE(r, 1);

    // Parse path
    l = r;
    while (is_valid_http_path_char(*r))
        SAFE_ADVANCE(r, 1);
    if (l == r) {
        req->error = "Invalid path";
        return req;
    }
    req->path = xstrndup(l, (size_t) (r - l));

    // Space
    if (*r != ' ') {
        req->error = "No space after path";
        return req;
    }
    SAFE_ADVANCE(r, 1);

    // Parse 'HTTP' part of the HTTP version
    if (strncmp(r, "HTTP", 4)) {
        req->error = "No 'HTTP' in HTTP version";
        return req;
    }
    SAFE_ADVANCE(r, 4);

    // Slash after 'HTTP'
    if (*r != '/') {
        req->error = "No '/' in HTTP version";
        return req;
    }
    SAFE_ADVANCE(r, 1);

    // Version number '[0-9]+\.[0-9]+'
    l = r;
    while (is_digit(*r))
        SAFE_ADVANCE(r, 1);
    if (*r != '.') {
        req->error = "No '.' in HTTP version number";
        return req;
    }
    SAFE_ADVANCE(r, 1);
    while (is_digit(*r))
        SAFE_ADVANCE(r, 1);
    req->version_number = xstrndup(l, (size_t) (r - l));
    if(!can_handle_http_ver(req->version_number)) {
        req->error = "Can't handle given version";
        return req;
    }

    // \r\n at the end
    if (*r != '\r' || *(r + 1) != '\n') {
        req->error = "No CRLF after HTTP version";
        return req;
    }
    SAFE_ADVANCE(r, 2);

    // Parse headers
    while (1) {
        char *hn; // header name
        size_t hn_len;
        char *hv; // header value
        size_t hv_len;

        // Final \r\n
        if (*r == '\r' && *(r + 1) == '\n') {
            return req;
        }

        // Header name
        hn = r;
        while (is_alpha(*r) || *r == '-')
            SAFE_ADVANCE(r, 1);
        hn_len = (size_t) (r - hn);

        if (*r != ':') {
            req->error = "No ':' after header name";
            return req;
        }
        SAFE_ADVANCE(r, 1);

        if (*r != ' ') {
            req->error = "No space after header name colon";
            return req;
        }
        SAFE_ADVANCE(r, 1);

        // Header value
        for(hv = r; !(*r == '\r' && *(r+1) == '\n');) {
            if (*r == '\0') {
                req->error = "No CRLF at the end of the request";
                return req;
            }
            SAFE_ADVANCE(r, 1);
        }
        hv_len = (size_t) (r - hv);

        // Step over \r\n
        SAFE_ADVANCE(r, 2);

        // Dump header
        /* for (size_t i = 0; i < hn_len; i++) */
        /*     fputc(hn[i], stdout); */
        /* fprintf(stdout, ": "); */
        /* for (size_t i = 0; i < hv_len; i++) */
        /*     fputc(hv[i], stdout); */
        /* fputc('\n', stdout); */

        // Check if we handle header
        if (!strncasecmp("Host:", hn, hn_len + 1)) {
            req->host = xstrndup(hv, hv_len);
        } else if (!strncasecmp("User-Agent:", hn, hn_len + 1)) {
            req->user_agent = xstrndup(hv, hv_len);
        } else if (!strncasecmp("Accept:", hn, hn_len + 1)) {
            req->accept = xstrndup(hv, hv_len);
        } else if (!strncasecmp("Connection:", hn, hn_len + 1)) {
            req->connection = xstrndup(hv, hv_len);
        } else if (!strncasecmp("Range:", hn, hn_len + 1)) {
            parse_range_header(hv, hv_len,
                               &req->range_start_given,
                               &req->range_start,
                               &req->range_end_given,
                               &req->range_end);
        }
    }

    return req;
    #undef SAFE_ADVANCE
}

void
free_http_request(Http_Request *req)
{
    if (!req) return;

    free(req->method);
    free(req->path);
    free(req->version_number);
    free(req->host);
    free(req->user_agent);
    free(req->accept);
    free_buf(req->buf);

    // NOTE: DO NOT free req->error as it's static
    free(req);
    req = NULL;
}

void
print_http_request(FILE *f, Http_Request *req)
{
    if (!req) {
        fprintf(f, "(Http_Request) NULL\n");
        return;
    }

    fprintf(f, "(Http_Request) {\n");
    fprintf(f, "  .method = \"%s\",\n", req->method);
    fprintf(f, "  .path = \"%s\",\n", req->path);
    fprintf(f, "  .version_number = \"%s\",\n", req->version_number);
    fprintf(f, "  .host = \"%s\",\n", req->host);
    fprintf(f, "  .user_agent = \"%s\",\n", req->user_agent);
    fprintf(f, "  .accept = \"%s\",\n", req->accept);
    fprintf(f, "  .error = \"%s\",\n", req->error);
    fprintf(f, "  .range_start_given = %d,\n", req->range_start_given);
    fprintf(f, "  .range_start = %zu,\n", req->range_start);
    fprintf(f, "  .range_end_given = %d,\n", req->range_end_given);
    fprintf(f, "  .range_end = %zu,\n", req->range_end);
    fprintf(f, "}\n");
}

int
are_ranges_satisfiable(Http_Request* req, off_t max_range)
{
    return
        (req->range_start_given && (req->range_start > max_range)) ||
        (req->range_end_given && (req->range_end > max_range))     ||
        (req->range_start_given && req->range_end_given &&
         (req->range_start < req->range_end));
}

void
file_list_to_html(Buffer *buf, char *endpoint, File_List *fl)
{
    buf_append_str(
        buf,
        "<!DOCTYPE html><html>"
        "<head><style>"
          "* { font-family: monospace; }\n"
          "table, h1 { border: none; margin: 1rem; }\n"
          "td { padding-right: 2rem; }\n"
          ".red { color: crimson; }\n"
        "</style></head>\n");

    // Show endpoint as header
    buf_sprintf(
        buf,
        "<body>\n"
        "<h1>%s</h1>\n"
        "<table>\n",
        endpoint);

    for (size_t i = 0; i < fl->len; i++) {
        File *f = fl->files + i;

        // Write file name
        buf_append_str(buf, "<tr><td><a href=\"");
        buf_encode_url(buf, f->name);
        if (f->is_dir) buf_push(buf, '/');
        buf_push(buf, '"');
        if (f->is_link && f->is_broken_link)
            buf_append_str(buf, " class=\"red\"");
        buf_push(buf, '>');
        buf_append_str(buf, f->name);
        buf_append_str(buf, get_file_type_suffix(f));

        // Write file size
        buf_append_str(buf, "</a></td><td>");
        if (!f->is_dir) {
            char *tmp = get_human_file_size(f->size);
            buf_append_str(buf, tmp);
            free(tmp);
        }
        buf_append_str(buf, "</td><td>");

        // Write file permissions
        char *tmp = get_human_file_perms(f);
        buf_append_str(buf, tmp);
        free(tmp);
        buf_append_str(buf, "</td></tr>\n");
    }

    buf_append_str(buf, "</table></body></html>\n");
}

// Writes dirlisting headers to given 'head' Buffer and
// the HTML to given 'body' Buffer.
// 'dir' is the directory on the machine.
// 'http_path' is the requested path extracted from the GET request.
void
write_dirlisting_http(
    Buffer *head,
    Buffer *body,
    char *dir,
    char *http_path)
{
    // Get file list
    File_List *fl = ls(dir);
    if (!fl) {
        // Internal error
        buf_append_str(head, "HTTP/1.1 500\r\n");
        return;
    }

    // TODO: append headers using a function which can be used
    // both here and in make_http_response()

    file_list_to_html(body, http_path, fl);

    char date_buf[DATE_LEN];
    to_rfc1123_date(date_buf, fl->dir_info->last_mod);

    buf_sprintf(
        head,
        "HTTP/1.1 200\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Last-Modified: %s\r\n\r\n",
        body->n_items,
        date_buf);

    free_file_list(fl);
    return;
}

Http_Response*
make_http_response(Server *serv, Http_Request *req)
{
    Defer_Queue dq = NULL_DEFER_QUEUE;

    Http_Response *res = xmalloc(sizeof(Http_Response));
    init_buf(&res->head, RESPONSE_HEADERS_BUF_INIT_SIZE);
    res->head_nbytes_sent = 0;

    int is_head_request = !strcmp(req->method, "HEAD");
    res->body.data = NULL;
    res->body_nbytes_sent = 0;

    res->file = NULL_FILE;
    res->file_offset = 0;
    res->file_nbytes_sent = 0;
    res->file_path = NULL;

    int is_range_given = req->range_start_given || req->range_end_given;

    res->error = NULL;

    char *clean_http_path = cleanup_path(req->path);
    char *decoded_http_path = decode_url(clean_http_path);
    free(clean_http_path);
    defer(&dq, free, decoded_http_path);

    // Real path to the file on the server
    char *real_path = resolve_path(serv->conf.serve_path, decoded_http_path);
    res->file_path = real_path;
    // NOTE: Don't free real_path, it's used outside this function

    // Find out if we're listing a dir or serving a file
    res->file.name = get_base_name(real_path);
    int read_result = read_file_info(&(res->file), real_path);

    // File not found
    if (read_result == -1) {
        char *body = "Error 404: file not found\n";

        // Status
        buf_append_str(&res->head, "HTTP/1.1 404\r\n");

        // Headers
        /* TODO: have a function like
            `write_standard_headers(buf, serv)` that puts
            Connection, Keep-Alive, Date headers into buf */
        buf_append_str(&res->head, "Accept-Ranges: bytes\r\n");
        buf_append_str(&res->head, "Content-Type: text/plain\r\n");
        buf_sprintf(&res->head, "Content-Length: %zu\r\n", strlen(body));
        /*buf_sprintf(res->buf, "Keep-Alive: timeout=%d\r\n",
            serv->conf.timeout_secs);*/
        buf_append_str(&res->head, "\r\n");

        // Body
        if (!is_head_request) {
            init_buf(&res->body, strlen(body) + 1);
            buf_append_str(&res->body, body);
        }
        return fulfill(&dq, res);
    }

    // Fatal error
    if (read_result == -2) {
        buf_append_str(&res->head, "HTTP/1.1 500\r\n\r\n");
        return fulfill(&dq, res);
    }

    // We're serving a dirlisting
    if (res->file.is_dir) {
        // Forward to path with trailing slash if it's missing
        if (decoded_http_path[strlen(decoded_http_path) - 1] != '/') {
            buf_sprintf(
                &res->head,
                "HTTP/1.1 301\r\n"
                "Location: %s/\r\n",
                decoded_http_path);
            buf_append_str(&res->head, "Accept-Ranges: bytes\r\n");
            /*buf_sprintf(
                &res->head,
                "Keep-Alive: timeout=%d\r\n\r\n",
                serv->conf.timeout_secs);*/
            return fulfill(&dq, res);
        }

        // FIXME: the code below is ugly (especially the real_path stuff)
        // Look for the index file if configured
        int index_found = 0;
        if (serv->conf.index != NULL) {
            char *index_real_path = resolve_path(real_path, serv->conf.index);
            free(res->file.name);
            res->file.name = xstrdup(serv->conf.index);
            int index_read_result = read_file_info(
                &(res->file),
                index_real_path);

            // Index file found, it will be served
            if (index_read_result == 1) {
                index_found = 1;
                free(real_path);
                real_path = index_real_path;
                res->file_path = index_real_path;
            } else {
                defer(&dq, free, index_real_path);
            }
        }

        if (index_found == 0) {
            init_buf(&res->body, RESPONSE_BODY_BUF_INIT_SIZE);
            write_dirlisting_http(
                &res->head,
                &res->body,
                real_path,
                decoded_http_path);

            // Check if the ranges given are invalid
            if (is_range_given &&
                !are_ranges_satisfiable(req, res->body.n_items)) {
                // TODO: return 'range cannot be satisfied' error
            }

            // Set ranges
            res->range_start = req->range_start_given ?
                req->range_start : 0;
            res->range_end = req->range_end_given ?
                req->range_end : (off_t) res->body.n_items - 1;

            return fulfill(&dq, res);
        }
    }

    // We're serving a single file

    // Set ranges and file_offset
    res->range_start = req->range_start_given ?
        req->range_start : 0;
    res->range_end = req->range_end_given ?
        req->range_end : (off_t) res->file.size - 1;
    res->file_offset = res->range_start;

    if (is_range_given) {
        if (!are_ranges_satisfiable(req, res->file.size)) {
            // TODO: return 'range cannot be satisfied' error
        }

        buf_append_str(&res->head, "HTTP/1.1 206\r\n");
    } else {
        buf_append_str(&res->head, "HTTP/1.1 200\r\n");
        buf_append_str(&res->head, "Accept-Ranges: bytes\r\n");
    }

    // Keep-Alive
    /*buf_sprintf(
        &res->head,
        "Keep-Alive: timeout=%d\r\n",
        serv->conf.timeout_secs);*/

    // Last-Modified
    char tmp[DATE_LEN * 10];
    buf_sprintf(&res->head,
                "Last-Modified: %s\r\n",
                to_rfc1123_date(tmp, res->file.last_mod));

    // Content-Range
    if (is_range_given) {
        buf_sprintf(&res->head,
            "Content-Range: bytes %ld-%ld/%ld\r\n",
            res->range_start, res->range_end, res->file.size);
    }

    // Content-Length
    buf_sprintf(&res->head,
                "Content-Length: %ld\r\n",
                res->file.size);

    // Content-Type
    // TODO: replace this with a table lookup
    if (strstr(res->file.name, ".html")) {
        buf_append_str(
            &res->head,
            "Content-Type: text/html; charset=UTF-8\r\n");
    } else if (strstr(res->file.name, ".jpg")) {
        buf_append_str(
            &res->head,
            "Content-Type: image/jpeg\r\n");
    } else if (strstr(res->file.name, ".pdf")) {
        buf_append_str(
            &res->head,
            "Content-Type: application/pdf\r\n");
    } else if (strstr(res->file.name, ".css")) {
        buf_append_str(
            &res->head,
            "Content-Type: text/css\r\n");
    } else if (strstr(res->file.name, ".txt")) {
        buf_append_str(
            &res->head,
            "Content-Type: text/plain; charset=UTF-8\r\n");
    } else if (strstr(res->file.name, ".mp4")) {
        buf_append_str(
            &res->head,
            "Content-Type: video/mp4\r\n");
    } else {
        buf_append_str(
            &res->head,
            "Content-Type: application/octet-stream; charset=UTF-8\r\n");
    }

    //ascii_dump_buf(stdout, res.data, res.n_items);

    // Last empty line after headers
    buf_append_str(&res->head, "\r\n");

    return fulfill(&dq, res);
}

void
print_http_response(FILE *stream, Http_Response *res)
{

    if (!res) {
        printf("Response empty!\n");
        return;
    }

    size_t n_items = 128;

    if (res->body.n_items < n_items) {
        n_items = res->body.n_items;
    }

    for (size_t i = 0; i < n_items; i++) {
        switch (res->body.data[i]) {
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
            putc(res->body.data[i], stream);
            break;
        }
    }

    if (res->body.n_items > n_items) {
        fprintf(stream,
                "\n[RESPONSE TRUNCATED]\n");
    }
}

void
free_http_response(Http_Response *res)
{
    if (!res) return;
    if (res->file_path) {
        free(res->file_path);
        res->file_path = NULL;
    }
    free_buf(&res->head);
    free_buf(&res->body);
    free_file(&res->file);
    free(res);
    res = NULL;
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

char*
decode_url(char *url)
{
    size_t len = strlen(url);
    char *ret = xmalloc(len + 1);
    char *p = ret;

    for (size_t i = 0; i < len; i++) {
        if (url[i] == '%') {
            if (i + 1 < len && url[i+1] == '%') {
                *p++ = '%';
                i++;
                continue;
            }

            if (i + 2 < len && is_hex(url[i+1]) && is_hex(url[i+2])) {
                *p = 0;

                // Frst hex digit
                if (url[i+1] >= 'A' && url[i+1] <= 'F') {
                    *p = 0x0A + (url[i+1] - 'A');
                } else if (url[i+1] >= 'a' && url[i+1] <= 'f') {
                    *p = 0x0A + (url[i+1] - 'a');
                } else if (url[i+1] >= '0' && url[i+1] <= '9') {
                    *p = url[i+1] - '0';
                }

                *p = *p << 4;

                // Second hex digit
                if (url[i+2] >= 'A' && url[i+2] <= 'F') {
                    *p |= (0x0A + (url[i+2] - 'A'));
                } else if (url[i+2] >= 'a' && url[i+2] <= 'f') {
                    *p |= (0x0A + (url[i+2] - 'a'));
                } else if (url[i+2] >= '0' && url[i+2] <= '9') {
                    *p |= url[i+2] - '0';
                }

                p++;
                i += 2;
                continue;
            }

        }

        *p++ = url[i];
    }

    *p = '\0';

    return ret;
}
