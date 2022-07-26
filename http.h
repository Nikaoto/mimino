#ifndef _MIMINO_HTTP_H
#define _MIMINO_HTTP_H

#include <stdio.h>
#include "mimino.h"
#include "buffer.h"

Http_Request* parse_http_request(Http_Request*);
int is_http_end(char *buf, size_t size);
void print_http_request(FILE*, Http_Request*);
void free_http_request(Http_Request*);

Http_Response* make_http_response(Server *serv, Http_Request* req);
void print_http_response(FILE*, Http_Response*);
void free_http_response(Http_Response*);

long long ll_power(long long a, long long b);
long long consume_next_num(char **str, char *end);

void buf_encode_url(Buffer *, char *);
char *decode_url(char *);

#endif // _MIMINO_HTTP_H
