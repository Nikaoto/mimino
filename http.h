#ifndef _MIMINO_HTTP_H
#define _MIMINO_HTTP_H

#include <stdio.h>
#include "buffer.h"

#define REQ_BUF_SIZE 1024
#define RES_BUF_SIZE 8192

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

typedef struct {
    //char *resolved_path;
    Buffer *buf;
    char *error;
} Http_Response;

Http_Request* parse_http_request(char*);
void print_http_request(FILE*, Http_Request*);
void free_http_request(Http_Request*);

Http_Response* construct_http_response(char *serve_path, Http_Request* req);
void print_http_response(FILE*, Http_Response*);
void free_http_response(Http_Response*);

#endif // _MIMINO_HTTP_H
