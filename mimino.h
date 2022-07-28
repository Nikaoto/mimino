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

#define CONN_STATE_READING          1
#define CONN_STATE_WRITING_HEADERS  2
#define CONN_STATE_WRITING_BODY     3
#define CONN_STATE_WRITING_FINISHED 4
#define CONN_STATE_CLOSING          5

#define MAX_REQUEST_SIZE               1<<12
#define RESPONSE_HEADERS_BUF_INIT_SIZE 1<<12
#define RESPONSE_BODY_BUF_INIT_SIZE    1<<12

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

    int range_start_given;
    off_t range_start;

    int range_end_given;
    off_t range_end;
} Http_Request;

typedef struct {
    Buffer head;
    size_t head_nbytes_sent;

    Buffer body;
    size_t body_nbytes_sent;

    File file;
    off_t file_offset;
    size_t file_nbytes_sent;
    char *file_path;

    /*
      The difference between file_offset and range_start is that
      file_offset tracks the point from which data transfer must
      continue on each send cycle and thus changes each time a
      chunk is sent, whereas range_start doesn't change and only
      defines the initial offset from the start of the file.
    */
    off_t range_start;
    off_t range_end;

    char *error;
} Http_Response;

typedef struct {
    int fd;
    struct pollfd *pollfd;
    Http_Request *req;
    Http_Response *res;
    int read_tries_left; // read_request tries left until force closing
    int write_tries_left; // write_response tries left until force closing
    int keep_alive;
    int state;
    time_t last_active;
} Connection;

typedef struct {
    struct pollfd *pollfds;
    Connection *conns;
    nfds_t n_conns;
    nfds_t n_conns_alloc;
} Poll_Queue;

typedef struct {
    int verbose;
    int quiet;
    int unsafe;
    int chroot;
    int serve_error_files;
    int timeout_secs;
    int poll_interval_ms;
    int max_fds;
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
