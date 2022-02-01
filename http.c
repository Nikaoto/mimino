#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "http.h"

inline int
is_upper_ascii(char c)
{
    return c >= 'A' && c <= 'Z';
}

inline int
is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline int
is_digit(char c)
{
    return c >= '0' && c <= '9';
}

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

// Parses string buf into a request struct
Http_Request*
parse_http_request(const char *buf)
{
    const char *p = buf;
    Http_Request *req = malloc(sizeof(Http_Request));
    memset(req, 0, sizeof(*req));
/*
  GET / HTTP/1.1\r\n
  Host: google.com\r\n
  User-Agent: curl/7.74.0\r\n
  Accept: *_/*\r\n
           ^ no underscore here
  \r\n
*/
    // Parse method
    while (is_upper_ascii(*buf))
        buf++;
    if (p == buf) {
        req->error = "Invalid HTTP method";
        return req;
    }
    req->method = strndup(p, (size_t) (buf - p));

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
    req->path = strndup(p, (size_t) (buf - p));

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
    req->version_number = strndup(p, (size_t) (buf - p));
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

    // TODO: parse headers
    // TODO: parse final double '\r\n'
    return req;
}

void
free_http_request(Http_Request *req)
{

    free(req->method);
    free(req->path);
    free(req->version_number);

    // NOTE: DO NOT free req->error as it's static
    free(req);
}

void
print_http_request(FILE *f, Http_Request *req)
{
    fprintf(f, "(Http_Request) {\n");
    fprintf(f, "  .method = %s,\n", req->method);
    fprintf(f, "  .path = %s,\n", req->path);
    fprintf(f, "  .version_number = %s,\n", req->version_number);
    fprintf(f, "  .error = %s,\n", req->error);
    fprintf(f, "}\n");
}
