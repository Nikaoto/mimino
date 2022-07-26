#ifndef _MIMINO_H
#define _MIMINO_H

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include "http.h"
#include "dir.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define HEXDUMP_DATA 1
#define DUMP_WIDTH 10

#define REQ_BUF_SIZE 1024
#define RES_BUF_SIZE 8192

#define CONN_STATUS_READING   1
#define CONN_STATUS_WRITING   2
#define CONN_STATUS_WAITING   3
#define CONN_STATUS_CLOSED    4

typedef struct {
    char *data;
    size_t n_items; // Number items in data
    size_t n_alloc; // Number of bytes allocated for data
} Buffer;

#define BUFFER_GROWTH 4096

typedef struct {
    int fd;
    struct pollfd *pollfd;
    // NOTE: both req and res will be changed to dynamic char buffers
    Http_Request *req;
    char req_buf[REQ_BUF_SIZE];
    size_t req_buf_i;           // Points to char up to which data was read
    int read_tries_left;        // read_request tries left until force closing
    Buffer *res_buf;
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
