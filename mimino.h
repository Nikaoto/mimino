#ifndef _MIMINO_H
#define _MIMINO_H

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include "dir.h"
#include "buffer.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define HEXDUMP_DATA 1
#define DUMP_WIDTH 10

#define CONN_STATUS_READING   1
#define CONN_STATUS_WRITING   2
#define CONN_STATUS_WAITING   3
#define CONN_STATUS_CLOSED    4

#define MAX_REQUEST_SIZE       4096
#define RESPONSE_BUF_INIT_SIZE 4096

typedef struct {
    Buffer *buf;
    char *method;
    char *path;
    char *version_number;
    //char *query;
    char *host;
    char *user_agent;
    char *accept;
    char *error;
} Http_Request;

typedef struct {
    Buffer *buf;
    size_t nbytes_sent;
    char *error;
} Http_Response;

typedef struct {
    int fd;
    struct pollfd *pollfd;
    Http_Request *req;
    Http_Response *res;
    int read_tries_left;        // read_request tries left until force closing
    int write_tries_left;       // write_response tries left until force closing
    int status;
} Connection;

typedef struct {
    int verbose;
    char *serve_path;
    char *index_path;
    char *port;
    int sock;
    char ip[INET6_ADDRSTRLEN];
    struct addrinfo addrinfo;
} Server;

#define MAX_CONN 20

typedef struct {
    struct pollfd pollfds[MAX_CONN];
    nfds_t pollfd_count;
    Connection conns[MAX_CONN];
} Poll_Queue;

#endif // _MIMINO_H
