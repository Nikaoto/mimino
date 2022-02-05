#ifndef _MIMINO_H
#define _MIMINO_H

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifndef HEXDUMP_DATA
#define HEXDUMP_DATA 1
#endif

#ifndef DUMP_WIDTH
#define DUMP_WIDTH 10
#endif

#define REQ_BUF_SIZE 1024
#define RES_BUF_SIZE 8192

#define CONN_STATUS_READING   1
#define CONN_STATUS_WRITING   2
#define CONN_STATUS_WAITING   3
#define CONN_STATUS_CLOSING   4
#define CONN_STATUS_CLOSED    5

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include "http.h"
#include "dir.h"

typedef struct {
    char *data;
    size_t n_items; // Number items in data
    size_t n_alloc; // Number of bytes allocated for data
} Buffer;

#define BUFFER_GROWTH 256

typedef struct {
    int fd;
    struct pollfd *pollfd;
    // NOTE: both req and res will be changed to dynamic char buffers
    Http_Request *req;
    char req_buf[REQ_BUF_SIZE];
    size_t req_buf_i;           // Points to char up to which data was read
    char res_buf[RES_BUF_SIZE];
    size_t res_buf_i;           // Points to char up to which data was sent
    int status;
    int read_tries_left;        // read_request tries left until force closing
    int write_tries_left;       // write_response tries left until force closing
} Connection;

typedef struct {
    char *cwd;
    char *serve_dir;
    char *port;
    int sock;
    char ip[INET6_ADDRSTRLEN];
    struct addrinfo addrinfo;
} Server;

typedef struct {
    struct pollfd pollfds[20];
    nfds_t pollfd_count;
    Connection conns[21]; // First conn is ignored
} Poll_Queue;

#endif // _MIMINO_H
