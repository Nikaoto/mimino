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

    // Left and right boundaries of a token
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
        //size_t hn_len;
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
        //hn_len = (size_t) (r - hn);

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
        if (!strncasecmp("Host", hn, 4)) {
            req->host = xstrndup(hv, hv_len);
        } else if (!strncasecmp("User-Agent", hn, 10)) {
            req->user_agent = xstrndup(hv, hv_len);
        } else if (!strncasecmp("Accept", hn, 6)) {
            req->accept = xstrndup(hv, hv_len);
        } else if (!strncasecmp("Connection", hn, 10)) {
            req->connection = xstrndup(hv, hv_len);
        }
    }

    return req;
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

    fprintf(f, "%s %s\n", req->method, req->path);
    return;

    fprintf(f, "(Http_Request) {\n");
    fprintf(f, "  .method = \"%s\",\n", req->method);
    fprintf(f, "  .path = \"%s\",\n", req->path);
    fprintf(f, "  .version_number = \"%s\",\n", req->version_number);
    fprintf(f, "  .host = \"%s\",\n", req->host);
    fprintf(f, "  .user_agent = \"%s\",\n", req->user_agent);
    fprintf(f, "  .accept = \"%s\",\n", req->accept);
    fprintf(f, "  .error = \"%s\",\n", req->error);
    fprintf(f, "}\n");
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
        buf_append_str(buf, "</td><td>\n");

        // Write file permissions
        char *tmp = get_human_file_perms(f);
        buf_append_str(buf, tmp);
        free(tmp);
    }

    buf_append_str(buf, "</table></body></html>\r\n");
}

void
buf_write_dirlisting_http(Buffer *buf, char *dir, char *http_path)
{
    // Get file list
    File_List *fl = ls(dir);
    if (!fl) {
        // Internal error
        buf_append_str(buf, "HTTP/1.1 500\r\n");
        return;
    }

    // TODO: append headers using a function which can be used
    // both here and in make_http_response()

    // Write the html into a separate buffer, so we can measure its length
    Buffer *html_buf = new_buf(RESPONSE_BUF_INIT_SIZE);
    file_list_to_html(html_buf, http_path, fl);

    char date_buf[DATE_LEN];
    to_rfc1123_date(date_buf, fl->dir_info->last_mod);

    buf_sprintf(
        buf,
        "HTTP/1.1 200\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n"
        "Last-Modified: %s\r\n\r\n",
        html_buf->n_items,
        date_buf);

    buf_append_buf(buf, html_buf);

    free_file_list(fl);
    free_buf(html_buf);
    return;
}

Http_Response*
make_http_response(Server *serv, Http_Request *req)
{
    Defer_Queue dq = NULL_DEFER_QUEUE;
    File file = NULL_FILE;

    Http_Response *res = xmalloc(sizeof(Http_Response));
    res->buf = new_buf(RESPONSE_BUF_INIT_SIZE);
    res->nbytes_sent = 0;
    res->error = NULL;

    char *clean_http_path = cleanup_path(req->path);
    char *decoded_http_path = decode_url(clean_http_path);
    free(clean_http_path);
    defer(&dq, free, decoded_http_path);

    // Real path to the file on the server
    char *real_path = resolve_path(serv->conf.serve_path, decoded_http_path);
    defer(&dq, free, real_path);

    /* printf("serve path: %s\n", serv->serve_path); */
    /* printf("request path: %s\n", req->path); */
    /* printf("decoded request path: %s\n", decoded_http_path); */
    /* printf("resolved path: %s\n", path); */

    // Find out if we're listing a dir or serving a file
    char *base_name = get_base_name(real_path);
    int read_result = read_file_info(&file, real_path, base_name);
    free(base_name);
    // FIXME: possible free() call on file.name which might never get allocated
    defer(&dq, (void (*)(void*))free_file_parts, &file);

    // File not found
    if (read_result == -1) {
        char *body = "Error 404: file not found\n";

        // Status
        buf_append_str(res->buf, "HTTP/1.1 404\r\n");

        // Headers
        /* TODO: have a function like
            `write_standard_headers(buf, serv)` that puts
            Connection, Keep-Alive, Date headers into buf */
        buf_append_str(res->buf, "Content-Type: text/plain\r\n");
        buf_sprintf(res->buf, "Content-Length: %zu\r\n", strlen(body));
        /*buf_sprintf(res->buf, "Keep-Alive: timeout=%d\r\n",
            serv->conf.timeout_secs);*/
        buf_append_str(res->buf, "\r\n");

        // Content
        buf_append_str(res->buf, body);
        return fulfill(&dq, res);
    }

    // Fatal error
    if (read_result == -2) {
        buf_append_str(res->buf, "HTTP/1.1 500\r\n\r\n");
        return fulfill(&dq, res);
    }

    // We're serving a dirlisting
    if (file.is_dir) {
        // Forward to path with trailing slash if it's missing
        if (decoded_http_path[strlen(decoded_http_path) - 1] != '/') {
            buf_sprintf(
                res->buf,
                "HTTP/1.1 301\r\n"
                "Location: %s/\r\n",
                decoded_http_path);
            /*buf_sprintf(
                res->buf,
                "Keep-Alive: timeout=%d\r\n\r\n",
                serv->conf.timeout_secs);*/
            return fulfill(&dq, res);
        }

        // Look for the index file if configured
        int index_found = 0;
        if (serv->conf.index != NULL) {
            char *index_real_path = resolve_path(real_path, serv->conf.index);
            defer(&dq, free, index_real_path);
            int index_read_result = read_file_info(
                &file,
                index_real_path,
                serv->conf.index);

            // Index file found
            if (index_read_result == 1) {
                index_found = 1;
                real_path = index_real_path;
            }
        }

        if (index_found == 0) {
            buf_write_dirlisting_http(res->buf, real_path, decoded_http_path);
            return fulfill(&dq, res);
        }
    }

    // We're serving a single file
    buf_append_str(res->buf, "HTTP/1.1 200\r\n");

    // Keep-Alive
    /*buf_sprintf(
        res->buf,
        "Keep-Alive: timeout=%d\r\n",
        serv->conf.timeout_secs);*/

    // Content-Type
    if (strstr(file.name, ".html")) {
        buf_append_str(
            res->buf,
            "Content-Type: text/html; charset=UTF-8\r\n");
    } else if (strstr(file.name, ".jpg")) {
        buf_append_str(
            res->buf,
            "Content-Type: image/jpeg\r\n");
    } else if (strstr(file.name, ".pdf")) {
        buf_append_str(
            res->buf,
            "Content-Type: application/pdf\r\n");
    } else if (strstr(file.name, ".css")) {
        buf_append_str(
            res->buf,
            "Content-Type: text/css\r\n");
    } else {
        buf_append_str(
            res->buf,
            "Content-Type: text/plain; charset=UTF-8\r\n");
    }

    // Content-Length
    buf_sprintf(res->buf,
                "Content-Length: %ld\r\n",
                file.size);

    // Last-Modified
    char tmp[DATE_LEN * 10];
    buf_sprintf(res->buf,
                "Last-Modified: %s\r\n",
                to_rfc1123_date(tmp, file.last_mod));

    //ascii_dump_buf(stdout, res.data, res.n_items);

    // Last empty line before content
    buf_append_str(res->buf, "\r\n");

    // Write file contents
    int code = buf_append_file_contents(res->buf, &file, real_path);
    if (code == -1 || code == 0) {
        // TODO: handle this failure better (with a 500 err code).
        fprintf(stderr,
                "buf_append_file_contents failed with code %i\n",
                code);
        return fulfill(&dq, res);
    }

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

    if (res->buf->n_items < n_items) {
        n_items = res->buf->n_items;
    }

    for (size_t i = 0; i < n_items; i++) {
        switch (res->buf->data[i]) {
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
            putc(res->buf->data[i], stream);
            break;
        }
    }

    if (res->buf->n_items > n_items) {
        fprintf(stream,
                "\n[RESPONSE TRUNCATED]\n");
    }
}

void
free_http_response(Http_Response *res)
{
    if (!res) return;

    free_buf(res->buf);
    // free(res->error);
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
