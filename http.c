#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "http.h"
#include "dir.h"
#include "ascii.h"
#include "xmalloc.h"
#include "buffer.h"
#include "defer.h"

inline int
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

// Parses string buf into a request struct
Http_Request*
parse_http_request(char *buf)
{
    char *p = buf;
    Http_Request *req = xmalloc(sizeof(Http_Request));
    memset(req, 0, sizeof(*req));

    // Parse method
    while (is_upper_ascii(*buf))
        buf++;
    if (p == buf) {
        req->error = "Invalid HTTP method";
        return req;
    }
    req->method = xstrndup(p, (size_t) (buf - p));

    // Space
    if (*buf != ' ') {
        req->error = "No space after HTTP method";
        return req;
    }
    buf++;

    // Parse path
    p = buf;
    while (is_valid_http_path_char(*buf))
        buf++;
    if (p == buf) {
        req->error = "Invalid path";
        return req;
    }
    req->path = xstrndup(p, (size_t) (buf - p));

    // Space
    if (*buf != ' ') {
        req->error = "No space after path";
        return req;
    }
    buf++;

    // Parse 'HTTP' part of the HTTP version
    if (strncmp(buf, "HTTP", 4)) {
        req->error = "No 'HTTP' in HTTP version";
        return req;
    }
    buf+=4;

    // Slash after 'HTTP'
    if (*buf != '/') {
        req->error = "No '/' in HTTP version";
        return req;
    }
    buf++;

    // Version number '[0-9]+\.[0-9]+'
    p = buf;
    while (is_digit(*buf))
        buf++;
    if (*buf != '.') {
        req->error = "No '.' in HTTP version number";
        return req;
    }
    buf++;
    while (is_digit(*buf))
        buf++;
    req->version_number = xstrndup(p, (size_t) (buf - p));
    if(!can_handle_http_ver(req->version_number)) {
        req->error = "Can't handle given version";
        return req;
    }

    // \r\n at the end
    if (*buf != '\r' || *(buf + 1) != '\n') {
        req->error = "No CRLF after HTTP version";
        return req;
    }
    buf += 2;

    // Parse headers
    while (1) {
        char *hn; // header name
        //size_t hn_len;
        char *hv; // header value
        size_t hv_len;

        // Final \r\n
        if (*buf == '\r' && *(buf + 1) == '\n') {
            return req;
        }
  
        // Header name
        hn = buf;
        while (is_alpha(*buf) || *buf == '-')
            buf++;
        //hn_len = (size_t) (buf - hn);
  
        if (*buf != ':') {
            req->error = "No ':' after header name";
            return req;
        }
        buf++;
  
        if (*buf != ' ') {
            req->error = "No space after header name colon";
            return req;
        }
        buf++;
  
        // Header value
        for(hv = buf; !(*buf == '\r' && *(buf+1) == '\n'); buf++) {
            if (*buf == '\0') {
                req->error = "No CRLF at the end of the request";
                return req;
            }
        }
        hv_len = (size_t) (buf - hv);

        // Step over \r\n
        buf += 2;

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

    // NOTE: DO NOT free req->error as it's static
    free(req);
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

// req_path is the path inside the HTTP request
void
file_list_to_html(Buffer *buf, char *req_path, File_List *fl)
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
        buf_append_href(buf, f, req_path);
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
    res->buf = new_buf(RES_BUF_SIZE);
    res->nbytes_sent = 0;
    res->error = NULL;

    char *path = resolve_path(serv->serve_path, req->path);
    defer(&dq, free, path);

    printf("serve path: %s\n", serv->serve_path);
    printf("request path: %s\n", req->path);
    printf("resolved path: %s\n", path);

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
        buf_append_str(res->buf, "HTTP/1.1 200\r\n\r\n");
        file_list_to_html(res->buf, req->path, fl);
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
}
