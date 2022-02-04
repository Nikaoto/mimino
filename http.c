#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "http.h"
#include "ascii.h"

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

    // Parse headers
    while (1) {
        const char *hn; // header name
        //size_t hn_len;
        const char *hv; // header value
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
            req->host = strndup(hv, hv_len);
        } else if (!strncasecmp("User-Agent", hn, 10)) {
            req->user_agent = strndup(hv, hv_len);
        } else if (!strncasecmp("Accept", hn, 6)) {
            req->accept = strndup(hv, hv_len);
        }
    }

    return req;
}

void
free_http_request(Http_Request *req)
{

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
