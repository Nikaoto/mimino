#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "http.h"
#include "dir.h"
#include "ascii.h"
#include "xmalloc.h"
#include "buffer.h"
#include "defer.h"

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
    fprintf(f, "  .method = %s,\n", req->method);
    fprintf(f, "  .path = %s,\n", req->path);
    fprintf(f, "  .version_number = %s,\n", req->version_number);
    fprintf(f, "  .host = %s,\n", req->host);
    fprintf(f, "  .user_agent = %s,\n", req->user_agent);
    fprintf(f, "  .accept = %s,\n", req->accept);
    fprintf(f, "  .error = %s,\n", req->error);
    fprintf(f, "}\n");
}

// dir_path is the path of the directory containing the files
void
file_list_to_html(Buffer *buf, char *dir_path, File_List *fl)
{
    buf_append_str(
        buf,
        "<!DOCTYPE html><html>"
        "<head><style>"
        "* { font-family: monospace; }\n"
        "table { border: none; margin: 1rem; }\n"
        "td { padding-right: 2rem; }\n"
        ".red { color: crimson; }\n"
        "</style></head>"
        "<body><table>\n");

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

Http_Response*
make_http_response(Server *serv, Http_Request *req)
{
    Defer_Queue dq = NULL_DEFER_QUEUE;
    File file;

    Http_Response *res = xmalloc(sizeof(Http_Response));
    res->buf = new_buf(RESPONSE_BUF_INIT_SIZE);
    res->nbytes_sent = 0;
    res->error = NULL;

    char *clean_http_path = cleanup_path(req->path);
    char *decoded_http_path = decode_url(clean_http_path);
    free(clean_http_path);
    defer(&dq, free, decoded_http_path);

    char *path = resolve_path(serv->serve_path, decoded_http_path);
    defer(&dq, free, path);

    /* printf("serve path: %s\n", serv->serve_path); */
    /* printf("request path: %s\n", req->path); */
    /* printf("decoded request path: %s\n", decoded_http_path); */
    /* printf("resolved path: %s\n", path); */

    // Find out if we're listing a dir or serving a file
    char *base_name = get_base_name(path);
    int read_result = read_file_info(&file, path, base_name);
    free(base_name);
    defer(&dq, free_file_parts, &file);

    // File not found
    if (read_result == -1) {
        printf("file %s not found\n", path);
        buf_append_str(res->buf, "HTTP/1.1 404\r\n\r\n");
        // TODO: buf_append_http_error_msg(404);
        buf_append_str(res->buf, "Error 404: file not found\r\n");
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
                "Location:%s/\r\n\r\n",
                decoded_http_path);
            return fulfill(&dq, res);
        }

        Defer_Queue dqfl = NULL_DEFER_QUEUE;

        // Get file list
        File_List *fl = ls(path);
        if (!fl) {
            // Internal error
            buf_append_str(res->buf, "HTTP/1.1 500\r\n");
            return fulfill(&dq, res);
        }
        defer(&dqfl, free_file_list, fl);

        // Write html into response buffer
        buf_append_str(res->buf, "HTTP/1.1 200\r\n");
        buf_append_str(
            res->buf,
            "Content-Type: text/html; charset=UTF-8\r\n\r\n");
        file_list_to_html(res->buf, decoded_http_path, fl);
        buf_append_str(res->buf, "\r\n");

        fulfill(&dqfl, NULL);
        return fulfill(&dq, res);
    }

    // We're serving a single file
    buf_append_str(res->buf, "HTTP/1.1 200\r\n");
    // printf("We're serving the file %s\n", file.name);

    // Write Content-Type header
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

    // Write Content-Length header
    buf_sprintf(res->buf,
                "Content-Length: %ld\r\n\r\n",
                file.size);
    //ascii_dump_buf(stdout, res.data, res.n_items);

    // Write file contents
    int code = buf_append_file_contents(res->buf, &file, path);
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
