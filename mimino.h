#ifndef _MIMINO_H
#define _MIMINO_H

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include "http.h"
#include "dir.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define HEXDUMP_DATA 1
#define DUMP_WIDTH 10

#define CONN_STATUS_READING   1
#define CONN_STATUS_WRITING   2
#define CONN_STATUS_WAITING   3
#define CONN_STATUS_CLOSED    4

typedef struct {
    int fd;
    struct pollfd *pollfd;
    // NOTE: both req and res will be changed to dynamic char buffers
    Http_Request *req;
    char req_buf[REQ_BUF_SIZE]; // TODO: replace with Buffer
    size_t req_buf_i;           // Points to char up to which data was read
    int read_tries_left;        // read_request tries left until force closing
    Http_Response *res;
    int write_tries_left;       // write_response tries left until force closing
    int status;
} Connection;

typedef struct {
    char *serve_path;
    char *index_path;
    char *port;
    int sock;
    char ip[INET6_ADDRSTRLEN];
    struct addrinfo addrinfo;
} Server;

typedef struct {
    struct pollfd pollfds[20];
    nfds_t pollfd_count;
    Connection conns[20];
} Poll_Queue;

#endif // _MIMINO_H
