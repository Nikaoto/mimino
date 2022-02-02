#ifndef _HTTP_H
#define _HTTP_H

#include <stdio.h>

typedef struct {
    char *method;
    char *path;
    char *version_number;
    //char *query;
    //char *fragment;
    char *host;
    char *user_agent;
    char *accept;
    char *error;
} Http_Request;

Http_Request* parse_http_request(const char *);
void print_http_request(FILE *, Http_Request *);
void free_http_request(Http_Request *);

#endif // _HTTP_H
