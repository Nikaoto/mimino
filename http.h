#ifndef _MIMINO_HTTP_H
#define _MIMINO_HTTP_H

#include <stdio.h>
#include "mimino.h"

Http_Request* parse_http_request(char*);
void print_http_request(FILE*, Http_Request*);
void free_http_request(Http_Request*);

Http_Response* construct_http_response(char *serve_path, Http_Request* req);
void print_http_response(FILE*, Http_Response*);
void free_http_response(Http_Response*);

#endif // _MIMINO_HTTP_H
