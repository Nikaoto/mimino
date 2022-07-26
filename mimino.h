#ifndef _MIMINO_H
#define _MIMINO_H

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include "dir.h"
#include "buffer.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define REQ_BUF_SIZE 8192
#define RES_BUF_SIZE 8192

#define HEXDUMP_DATA 1
#define DUMP_WIDTH 10

#define CONN_STATUS_READING   1
#define CONN_STATUS_WRITING   2
#define CONN_STATUS_WAITING   3
#define CONN_STATUS_CLOSED    4

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
    size_t nbytes_sent;
    char *error;
} Http_Response;

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

#define MAX_CONN 20

typedef struct {
    struct pollfd pollfds[MAX_CONN];
    nfds_t pollfd_count;
    Connection conns[MAX_CONN];
} Poll_Queue;

#endif // _MIMINO_H
