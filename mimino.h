#ifndef _MIMINO_H
#define _MIMINO_H

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <time.h>
#include "dir.h"
#include "buffer.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define HEXDUMP_DATA 1
#define DUMP_WIDTH 10

#define CONN_STATUS_READING   1
#define CONN_STATUS_WRITING   2
#define CONN_STATUS_WAITING   3
#define CONN_STATUS_TIMED_OUT 4
#define CONN_STATUS_CLOSED    5

#define TIMEOUT_SECS           20
#define POLL_TIMEOUT_MS        1000
#define MAX_REQUEST_SIZE       1<<12
#define RESPONSE_BUF_INIT_SIZE 1<<12

typedef struct {
    Buffer *buf;
    char *method;
    char *path;
    char *version_number;
    //char *query;
    char *host;
    char *user_agent;
    char *accept;
    char *connection;
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
    int keep_alive;
    int status;
    time_t last_active;
} Connection;

#define MAX_CONN 20

typedef struct {
    struct pollfd pollfds[MAX_CONN];
    nfds_t pollfd_count;
    Connection conns[MAX_CONN];
} Poll_Queue;

typedef struct {
    int verbose;
    int quiet;
    int unsafe;
    int chroot;
    int serve_error_files;
    char *serve_path;
    char *port;
    char *index;  // TODO: replace with array of strings 'index_list'
    char *suffix; // TODO: replace with array of strings 'suffix_list'
    char *chroot_dir;
} Server_Config;

typedef struct {
    Server_Config conf;
    Poll_Queue queue;
    time_t time_now;
    int sock;
    char ip[INET6_ADDRSTRLEN];
    struct addrinfo addrinfo;
} Server;

#endif // _MIMINO_H
