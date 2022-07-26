/*
  Mimino - small http server
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <dirent.h>
#include <time.h>
#include "mimino.h"
#include "xmalloc.h"
#include "defer.h"
#include "http.h"
#include "arg.h"
#include "connection.h"

int sockbind(struct addrinfo *ai);
int send_buf(int sock, char *buf, size_t nbytes);
void *get_in_addr(struct sockaddr *sa);
unsigned short get_in_port(struct sockaddr *sa);
void hex_dump_line(FILE *stream, char *buf, size_t buf_size, size_t width);
void dump_data(FILE *stream, char *buf, size_t bufsize, size_t nbytes_recvd);
void ascii_dump_buf(FILE *stream, char *buf, size_t buf_size);

void
print_server_config(Server_Config *conf)
{
    if (!conf) {
        printf("(Server_Config) (NULL)\n");
        return;
    }

    printf("(Server_Config) = {\n");
    printf("  .verbose = %d,\n", conf->verbose);
    printf("  .quiet = %d,\n", conf->quiet);
    printf("  .unsafe = %d,\n", conf->unsafe);
    printf("  .chroot = %d,\n", conf->chroot);
    printf("  .serve_error_files = %d,\n", conf->serve_error_files);
    printf("  .serve_path = \"%s\",\n", conf->serve_path);
    printf("  .port = \"%s\",\n", conf->port);
    printf("  .index = \"%s\",\n", conf->index);
    printf("  .suffix = \"%s\",\n", conf->suffix);
    printf("  .chroot_dir = \"%s\",\n", conf->chroot_dir);
    printf("  .timeout_secs = %d,\n", conf->timeout_secs);
    printf("  .poll_interval_ms = %d,\n", conf->poll_interval_ms);
    printf("}\n");
}

void
print_server_state(Server *serv)
{
    printf("time_now: %ld\n", serv->time_now);
    printf("pollfd_count: %zu\n", serv->queue.pollfd_count);
    for (nfds_t i = 1; i < serv->queue.pollfd_count; i++) {
        printf("Connection %zu:\n", i);
        print_connection(serv->queue.pollfds + i,
                         serv->queue.conns + i);
        printf("------------\n");
    }
}

int
init_server(char *port, struct addrinfo *server_addrinfo)
{
    int sockfd;
    char ip_str[INET6_ADDRSTRLEN];

    struct addrinfo *getaddrinfo_res;
    struct addrinfo **ipv4_addrinfos = NULL;
    int ipv4_addrinfos_i = 0;
    struct addrinfo **ipv6_addrinfos = NULL;
    int ipv6_addrinfos_i = 0;

    // Init hints for getaddrinfo
    struct addrinfo hints = {
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    // Get server info
    int err = getaddrinfo(NULL, port, &hints, &getaddrinfo_res);
    if (err) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(err));
        return 1;
    }

    // Populate ipv4_addrinfos and ipv6_addrinfos arrays
    for (struct addrinfo *rp = getaddrinfo_res; rp != NULL; rp = rp->ai_next) {
        // Log each getaddrinfo response
        if (inet_ntop(rp->ai_family,
                      get_in_addr(rp->ai_addr),
                      ip_str, sizeof(ip_str))) {
            printf("getaddrinfo response: %s:%s\n", ip_str, port);
        }

        switch (rp->ai_family) {
        case AF_INET:
            ipv4_addrinfos = xrealloc(ipv4_addrinfos,
                                     (ipv4_addrinfos_i + 1) * sizeof(ipv4_addrinfos));
            ipv4_addrinfos[ipv4_addrinfos_i] = rp;
            ipv4_addrinfos_i++;
            break;
        case AF_INET6:
            ipv6_addrinfos = xrealloc(ipv6_addrinfos,
                                     (ipv6_addrinfos_i + 1) * sizeof(ipv6_addrinfos));
            ipv6_addrinfos[ipv6_addrinfos_i] = rp;
            ipv6_addrinfos_i++;
            break;
        default:
            fprintf(stderr, "Unknown AF (addrinfo family): %d\n", rp->ai_family);
            continue;
        }
    }

    int bound = 0;

    // Try binding to ipv4 (prefer ipv4 over ipv6)
    for (int i = 0; i < ipv4_addrinfos_i; i++) {
        sockfd = sockbind(ipv4_addrinfos[i]);
        if (sockfd != -1) {
            memcpy(server_addrinfo, ipv4_addrinfos[i], sizeof(*server_addrinfo));
            bound = 1;
            break;
        }
    }

    // Try binding to ipv6
    if (!bound) {
        for (int i = 0; i < ipv6_addrinfos_i; i++) {
            sockfd = sockbind(ipv6_addrinfos[i]);
            if (sockfd != -1) {
                memcpy(server_addrinfo, ipv6_addrinfos[i], sizeof(*server_addrinfo));
                bound = 1;
                break;
            }
        }
    }

    free(ipv4_addrinfos);
    free(ipv6_addrinfos);
    freeaddrinfo(getaddrinfo_res);

    return bound ? sockfd : -1;
}

// Returns the socket fd of the new connection
int
accept_new_conn(int listen_sock)
{
    int newsock;
    int saved_errno;
    char their_ip_str[INET6_ADDRSTRLEN];
    struct sockaddr_storage their_addr;
    socklen_t their_addr_size = sizeof(their_addr);

    // Accept new connection
    // TODO: set O_NONBLOCK and check for EWOULDBLOCK. Right now this will block
    newsock = accept(listen_sock, (struct sockaddr *)&their_addr, &their_addr_size);
    saved_errno = errno;
    if (newsock == -1) {
        if (saved_errno != EAGAIN) {
            fprintf(stderr, "%d", saved_errno);
            perror("accept()");
        }
    }

    // Don't block on newsock
    if (fcntl(newsock, F_SETFL, O_NONBLOCK) != 0) {
        perror("fcntl()");
    }

    // Print their_addr
    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *)&their_addr),
              their_ip_str, sizeof(their_ip_str));
    /* printf("Got connection from %s:%d\n", their_ip_str, */
    /*        ntohs(get_in_port((struct sockaddr *)&their_addr))); */

    return newsock;
}

#define R_CLIENT_CLOSED 2 // client closed connection.
#define R_COMPLETE_READ 1 // done reading completely.
#define R_PARTIAL_READ  0 // an incomplete read happened.
#define R_FATAL_ERROR  -1 // fatal error or max retry reached.
#define R_REQ_TOO_BIG  -2 // when request is too big to handle.
int
read_request(Connection *conn)
{
    int n = recv(conn->fd,
                 conn->req->buf->data + conn->req->buf->n_items,
                 conn->req->buf->n_alloc - conn->req->buf->n_items,
                 0);
    if (n < 0) {
        int saved_errno = errno;
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
            if (conn->read_tries_left == 0) {
                fprintf(stderr, "Reached max read tries for conn\n");
                return R_FATAL_ERROR;
            }
            conn->read_tries_left--;
        }
        errno = saved_errno;
        perror("recv()");
        return R_PARTIAL_READ;
    }

    //dump_data(stdout,
    //          conn->req->buf->data + conn->req->buf->n_items,
    //          n,
    //          DUMP_WIDTH);

    conn->req->buf->n_items += n;

    // Finished reading
    if (is_http_end(conn->req->buf->data, conn->req->buf->n_items))
        return R_COMPLETE_READ;
    if (n == 0) {
        if (conn->req->buf->n_items == 0) return R_CLIENT_CLOSED;
        else return R_COMPLETE_READ;
    }

    // Not finished yet, but req buffer full
    if (conn->req->buf->n_items == conn->req->buf->n_alloc)
        return R_REQ_TOO_BIG;


    // TODO: maybe decrease read_tries_left here? imagine that a
    // client sends 10 bytes with 1 second delays (can happen
    // with slow networks), should we block them if they fail to
    // send their request in more than 5 tries? Otherwise, a
    // person with a larger memory than the server can simply
    // create 1000s of very slow requests and clog up the server.
    // Solution to this would be to have a
    // conn->reading_abs_timeout_ms which is set to 30000, which
    // means the client has 30 seconds to make the request in
    // total.
    // TODO: Could be smart to have conn->writing_abs_timeout_ms
    // as well.
    return R_PARTIAL_READ;
}

#define W_TIMED_OUT      -3
#define W_MAX_TRIES      -2
#define W_FATAL_ERROR    -1
#define W_PARTIAL_WRITE   0
#define W_COMPLETE_WRITE  1
int
write_headers(Connection *conn)
{
    int sent = send_buf(
        conn->fd,
        conn->res->head.data + conn->res->head_nbytes_sent,
        conn->res->head.n_items - conn->res->head_nbytes_sent);

    // Fatal error, no use retrying
    if (sent == -1) {
        conn->res->error = "write_headers(): Other error";
        return W_FATAL_ERROR;
    }

    conn->res->head_nbytes_sent += sent;

    // Retry later if not sent fully
    if (conn->res->head_nbytes_sent < conn->res->head.n_items) {
        if (conn->write_tries_left == 0) {
            conn->res->error = "write_headers(): Max write tries reached";
            return W_MAX_TRIES;
        }
        conn->write_tries_left--;

        return W_PARTIAL_WRITE;
    }

    return W_COMPLETE_WRITE;
}

int
write_body(Connection *conn)
{
    if (conn->res->body.data == NULL) {
        printf("Eror: Response for conn with fd %d has empty body\n",
            conn->fd);
        return W_FATAL_ERROR;
    }

    int sent = send_buf(
        conn->fd,
        conn->res->body.data +
          (conn->res->range_start + conn->res->body_nbytes_sent),
        conn->res->range_end + 1 - conn->res->body_nbytes_sent);

    // Fatal error, no use retrying
    if (sent == -1) {
        conn->res->error = "write_body(): Other error";
        return W_FATAL_ERROR;
    }

    conn->res->body_nbytes_sent += sent;

    // Retry later if not sent fully
    if (conn->res->range_start + (off_t) conn->res->body_nbytes_sent <
        conn->res->range_end + 1) {
        if (conn->write_tries_left == 0) {
            conn->res->error = "write_body(): Max write tries reached";
            return W_MAX_TRIES;
        }
        conn->write_tries_left--;

        return W_PARTIAL_WRITE;
    }

    return W_COMPLETE_WRITE;
}

int
write_file(Connection *conn)
{
    Defer_Queue dq = NULL_DEFER_QUEUE;

    if (conn->res->file.is_null) {
        printf("Eror: File is null for conn with fd %d\n",
            conn->fd);
        return W_FATAL_ERROR;
    }

    char file_buf[4096];
    size_t file_buf_len = sizeof(file_buf) * sizeof(char);
    size_t nbytes_left = conn->res->range_end - conn->res->file_offset;
    size_t nbytes_to_read = file_buf_len < nbytes_left ?
        file_buf_len : nbytes_left;

    // TODO: reuse file handle if partial file writes happen
    // Open file
    FILE *file_handle = fopen(conn->res->file_path, "r");
    if (file_handle == NULL) {
        perror("write_file(): Error on fopen()");
        // TODO: handle various fopen() errors by switching errno here
        return W_FATAL_ERROR;
    }
    defer(&dq, fclose, file_handle);

    // Seek
    if (conn->res->file_offset != 0) {
        int err = fseeko(file_handle, conn->res->file_offset, SEEK_SET);
        if (err) {
            perror("write_file(): Error on fseeko()");
            // TODO: handle various fseeko() errors by switching errno here
            return fulfill(&dq, W_FATAL_ERROR);
        }
    }

    // Start reading the file and sending it.
    // This works without loading the entire file into memory.
    while (1) {
        size_t bytes_read = fread(
            file_buf,
            1,
            nbytes_to_read,
            file_handle);
        if (bytes_read == 0) {
            if (ferror(file_handle)) {
                conn->res->error = "write_file(): Error when fread()ing file";
                // TODO: think of an appropriate error to return here
                return fulfill(&dq, W_FATAL_ERROR);
            }
            return W_COMPLETE_WRITE;
        }

        int sent = send_buf(conn->fd, file_buf, bytes_read);
        if (sent == -1) {
            conn->res->error = "write_file(): send_buf() returned -1";
            return fulfill(&dq, W_FATAL_ERROR);
        }

        conn->res->file_offset += sent;
        conn->res->file_nbytes_sent += sent; // TODO: useless?

        // Retry later if not sent at all

        // TODO: a better mechanic for dropping unresponsive
        // clients is to time the activity: if the client doesn't
        // receive any data for x seconds, drop the connection
        if ((size_t) sent == 0) {
            // This only happens when the client has a sudden
            // disconnection. Retrying later gives the client
            // some time to regain the connection.

            if (conn->write_tries_left == 0) {
                conn->res->error = "write_file(): Max write tries reached";
                return fulfill(&dq, W_MAX_TRIES);
            }
            conn->write_tries_left--;

            return fulfill(&dq, W_PARTIAL_WRITE);
        } else {
            conn->write_tries_left = 5;
        }
    };

    // Unreachable
    return fulfill(&dq, W_FATAL_ERROR);
}

void
close_connection(Server *s, nfds_t i)
{
    /*
      No need to check for errno here as most sane OSes close the
      file descriptor early in their close syscall. So, retrying
      with the same fd might close some other file. For more info
      read close(2) manual.
    */
    if (close(s->queue.pollfds[i].fd) == -1) {
        perror("close()");
    }

    s->queue.conns[i].fd = -1;
    s->queue.pollfds[i].fd = -1;
    s->queue.pollfds[i].events = 0;
    s->queue.pollfds[i].revents = 0;

    // Copy last pollfd and conn onto current pollfd and conn
    if (i != s->queue.pollfd_count - 1) {
        s->queue.pollfds[i] = s->queue.pollfds[s->queue.pollfd_count - 1];
        s->queue.conns[i] = s->queue.conns[s->queue.pollfd_count - 1];
    }

    s->queue.pollfd_count--;
}

void
recycle_connection(Server *s, nfds_t i)
{
    free_http_request(s->queue.conns[i].req);
    free_http_response(s->queue.conns[i].res);

    s->queue.conns[i].req = NULL;
    s->queue.conns[i].res = NULL;
    s->queue.conns[i].read_tries_left = 5;
    s->queue.conns[i].write_tries_left = 5;
    s->queue.conns[i].keep_alive = 1;
    s->queue.conns[i].last_active = s->time_now;

    s->queue.pollfds[i].events = POLLIN;
    s->queue.pollfds[i].revents = 0;
}

// TODO: don't take pollfd pointer, reference it from conn->
void
set_conn_state(struct pollfd *pfd, Connection *conn, int state)
{
    conn->state = state;

    switch (state) {
    case CONN_STATE_READING:
        pfd->events = POLLIN;
        break;
    case CONN_STATE_WRITING_HEADERS:
    case CONN_STATE_WRITING_BODY:
        pfd->events = POLLOUT;
        break;
    case CONN_STATE_WRITING_FINISHED:
    case CONN_STATE_CLOSING:
        pfd->events = 0;
        break;
    }
}

// State machine for handling connections
int
do_conn_state(Server *serv, nfds_t idx)
{
    Connection *conn = &(serv->queue.conns[idx]);
    struct pollfd *pfd = &(serv->queue.pollfds[idx]);

    switch (conn->state) {

    case CONN_STATE_READING: {
        if (!(pfd->revents & POLLIN))
            return 0;

        // Allocate memory for request struct
        if (conn->req == NULL) {
            conn->req = xmalloc(sizeof(*(conn->req)));
            memset(conn->req, 0, sizeof(*(conn->req)));
            // We won't grow this buffer
            conn->req->buf = new_buf(MAX_REQUEST_SIZE);
        }

        int status = read_request(conn);
        switch (status) {

        case R_PARTIAL_READ:
            conn->last_active = serv->time_now;
            break;

        case R_FATAL_ERROR:
        case R_CLIENT_CLOSED:
            set_conn_state(pfd, conn, CONN_STATE_CLOSING);
            do_conn_state(serv, idx);
            break;

        case R_REQ_TOO_BIG:
            // TODO: send 413 error instead
            set_conn_state(pfd, conn, CONN_STATE_CLOSING);
            do_conn_state(serv, idx);
            break;

        case R_COMPLETE_READ:
            conn->last_active = serv->time_now;

            // Reading done, parse request
            // TODO: turn this into a separate state
            parse_http_request(conn->req);

            print_http_request(stdout, conn->req);

            // Parse error
            if (conn->req->error) {
                fprintf(stdout,
                        "Parse error: %s\n",
                        conn->req->error);
                fprintf(stderr,
                        "Request buffer dump:\n");
                print_buf_ascii(stderr, conn->req->buf);

                // Close the connection if we didn't manage
                // to parse the essential headers
                if (!conn->req->method ||
                    !conn->req->path ||
                    !conn->req->host) {
                    set_conn_state(pfd, conn, CONN_STATE_CLOSING);
                    do_conn_state(serv, idx);
                    return 1;
                }
            }

            // Set keep-alive
            if (conn->req->connection &&
                !strcasecmp(conn->req->connection, "close")) {
                conn->keep_alive = 0;
            }

            // Start writing
            set_conn_state(pfd, conn, CONN_STATE_WRITING_HEADERS);
            break;
        }
        break;
    }

    case CONN_STATE_WRITING_HEADERS: {
        if (!(pfd->revents & POLLOUT))
            return 0;

        // Generate response
        if (!conn->res) {
            conn->last_active = serv->time_now;
            conn->res = make_http_response(serv, conn->req);

            if (serv->conf.verbose) {
                printf("-----------------\n");
                print_http_request(stdout, conn->req);
                print_http_response(stdout, conn->res);
                printf("-----------------\n");
            }
        }

        int status = write_headers(conn);
        switch (status) {
        case W_PARTIAL_WRITE:
            conn->last_active = serv->time_now;
            return 0;

        case W_MAX_TRIES:
            if (serv->conf.verbose) {
                printf("DEB: write_headers() on connection %lud "
                       "returned W_MAX_TRIES with error \"%s\"\n",
                       idx, conn->res->error);
            }
            set_conn_state(pfd, conn, CONN_STATE_CLOSING);
            do_conn_state(serv, idx);
            break;

        case W_FATAL_ERROR:
            if (serv->conf.verbose) {
                printf("DEB: write_headers() on connection %lud "
                       "returned W_FATAL_ERROR with error \"%s\"\n",
                       idx, conn->res->error);
            }
            set_conn_state(pfd, conn, CONN_STATE_CLOSING);
            do_conn_state(serv, idx);
            break;

        case W_COMPLETE_WRITE:
            if (!strcmp(conn->req->method, "HEAD")) {
                set_conn_state(pfd, conn, CONN_STATE_WRITING_FINISHED);
                do_conn_state(serv, idx);
            } else {
                set_conn_state(pfd, conn, CONN_STATE_WRITING_BODY);
                do_conn_state(serv, idx);
            }
            break;
        }
        break;
    }

    case CONN_STATE_WRITING_BODY: {
        if (!(pfd->revents & POLLOUT))
            return 0;

        int write_fn; // If write_body was called, holds 0, otherwise holds 1
        int status;
        if (conn->res->body.data) {
            status = write_body(conn);
            write_fn = 0;
        } else {
            status = write_file(conn);
            write_fn = 1;
        }
        switch (status) {
        case W_PARTIAL_WRITE:
            return 0;

        case W_MAX_TRIES:
            // TODO: print error stuff
            printf("W_MAX_TRIES\n");
            set_conn_state(pfd, conn, CONN_STATE_CLOSING);
            do_conn_state(serv, idx);
            break;

        case W_FATAL_ERROR:
            // TODO: print error stuff
            if (serv->conf.verbose) {
                if (write_fn == 0) {
                    printf("ERR: write_file() on connection %lud "
                           "returned W_FATAL_ERROR with error \"%s\"\n",
                           idx, conn->res->error);
                } else {
                    printf("ERR: write_body() on connection %lud "
                           "returned W_FATAL_ERROR with error \"%s\"\n",
                           idx, conn->res->error);
                }
            }
            set_conn_state(pfd, conn, CONN_STATE_CLOSING);
            do_conn_state(serv, idx);
            break;

        case W_COMPLETE_WRITE:
            set_conn_state(pfd, conn, CONN_STATE_WRITING_FINISHED);
            do_conn_state(serv, idx);
            break;
        }
        break;
    }

    case CONN_STATE_WRITING_FINISHED: {
        if (conn->keep_alive) {
            recycle_connection(serv, idx);
            set_conn_state(pfd, conn, CONN_STATE_READING);
        } else {
            set_conn_state(pfd, conn, CONN_STATE_CLOSING);
            do_conn_state(serv, idx);
        }
        break;
    }

    case CONN_STATE_CLOSING:
        free_connection_parts(conn);
        close_connection(serv, idx);
        break;
    }

    return -1;
}

int
main(int argc, char **argv)
{
    Server serv = {0};
    Argdef argdefs[9];
    memset(argdefs, 0, sizeof(argdefs));
    argdefs[0] = (Argdef) {
        .short_arg = 'v',
        .long_arg = "verbose",
        .type = ARGDEF_TYPE_BOOL,
    };
    argdefs[1] = (Argdef) {
        .short_arg = 'q',
        .long_arg = "quiet",
        .type = ARGDEF_TYPE_BOOL,
    };
    argdefs[2] = (Argdef) {
        .short_arg = 'u',
        .long_arg = "unsafe",
        .type = ARGDEF_TYPE_BOOL,
    };
    argdefs[3] = (Argdef) {
        .short_arg = 'r',
        .long_arg = "chroot",
        .type = ARGDEF_TYPE_STRING,
    };
    argdefs[4] = (Argdef) {
        .short_arg = 'e',
        .long_arg = "error-files",
        .type = ARGDEF_TYPE_BOOL,
    };
    argdefs[5] = (Argdef) {
        .short_arg = 's',
        .long_arg = "suffix",
        .type = ARGDEF_TYPE_STRING,
    };
    argdefs[6] = (Argdef) {
        .short_arg = 'p',
        .long_arg = "port",
        .type = ARGDEF_TYPE_STRING,
    };
    argdefs[7] = (Argdef) {
        .short_arg = 'i',
        .long_arg = "index",
        .type = ARGDEF_TYPE_STRING,
    };
    argdefs[8] = (Argdef) {
        // file/directory to serve
        .type = ARGDEF_TYPE_RAW,
    };

    int parse_ok = parse_args(argc, argv, 9, argdefs);
    if (!parse_ok) {
        printf("Error parsing arguments\n");
        return 1;
    }

    // Set server configs
    serv.conf = (Server_Config) {
        .verbose = argdefs[0].bvalue,
        .quiet = argdefs[1].bvalue,
        .unsafe = argdefs[2].bvalue && !argdefs[3].bvalue,
        .chroot = argdefs[3].bvalue,
        .chroot_dir = argdefs[3].value,
        .serve_error_files = argdefs[4].bvalue,
        .port = argdefs[6].value ? argdefs[6].value : "8080",
        .suffix = argdefs[5].bvalue ?
            (argdefs[5].value ? argdefs[5].value : ".html")
            : NULL,
        .index = argdefs[7].bvalue ?
            (argdefs[7].value ? argdefs[7].value : "index.html")
            : NULL,
        .serve_path = argdefs[8].value ? argdefs[8].value : "./",

        .timeout_secs     = 20,
        .poll_interval_ms = 1000,
    };

    // Set chroot directory
    if (serv.conf.chroot && serv.conf.chroot_dir == NULL) {
        serv.conf.chroot_dir = serv.conf.serve_path;
    }

    // Print server configuration
    if (!serv.conf.quiet) print_server_config(&serv.conf);

    // Init server
    serv.time_now = time(NULL);
    char ip_str[INET6_ADDRSTRLEN];
    struct addrinfo server_addrinfo = {0};
    int listen_sock = init_server(serv.conf.port, &server_addrinfo);
    if (listen_sock == -1) {
        fprintf(stderr, "init_server() failed.\n");
        return 1;
    }

    // Print server IP. Exit if it's munged
    if (inet_ntop(server_addrinfo.ai_family,
                  get_in_addr(server_addrinfo.ai_addr),
                  ip_str, sizeof(ip_str)) == NULL) {
        perror("inet_ntop()");
        return 1;
    }
    printf("Bound to %s:%s\n", ip_str, serv.conf.port);

    // Start listening
    int backlog = 10;
    if (listen(listen_sock, backlog) == -1) {
        perror("listen()");
        return 1;
    }

    // Add listen_sock to poll queue
    serv.queue.pollfds[0] = (struct pollfd) {
        .fd = listen_sock,
        .events = POLLIN,
        .revents = 0,
    };
    serv.queue.pollfd_count = 1;
    // NOTE: This connection won't be used; it's the listen socket
    serv.queue.conns[0] = make_connection(listen_sock, &serv, 0);

    // Main loop
    while (1) {
        serv.time_now = time(NULL);
        int nfds = poll(
            serv.queue.pollfds,
            serv.queue.pollfd_count,
            serv.conf.poll_interval_ms);

        if (nfds == -1) {
            perror("poll() returned -1");
            return 1;
        }

        if (serv.conf.verbose) {
            printf("\n\nSERVER STATE BEFORE ITERATION:\n");
            print_server_state(&serv);
        }

        // Accept new connection
        // pollfds[0].fd is the listen_sock
        if (serv.queue.pollfds[0].revents & POLLIN) {
            if (serv.queue.pollfd_count >= MAX_CONN) {
                fprintf(
                    stderr,
                    "poll queue reached maximum capacity of %d\n",
                    MAX_CONN);
            } else {
                int newsock = accept_new_conn(listen_sock);
                if (newsock != -1) {
                    // Update poll queue
                    serv.queue.pollfd_count++;
                    int i = serv.queue.pollfd_count - 1;
                    serv.queue.pollfds[i] = (struct pollfd) {
                        .fd = newsock,
                        .events = POLLIN,
                        .revents = 0,
                    };
                    serv.queue.conns[i] =
                        make_connection(newsock, &serv, i);

                    // Restart loop to prioritize new connections
                    continue;
                }
            }
        }

        // Iterate conn/pollfd queue (starts from 1, skipping listen_sock)
        for (nfds_t idx = 1; idx < serv.queue.pollfd_count; idx++) {
            Connection *conn = &(serv.queue.conns[idx]);

            // Drop connection if it timed out
            if ((conn->state != CONN_STATE_CLOSING) &&
                (conn->last_active + serv.conf.timeout_secs
                   <= serv.time_now)) {
                fprintf(stdout, "Connection %ld timed out\n", idx);
                conn->state = CONN_STATE_CLOSING;
            }

            do_conn_state(&serv, idx);
        }

        if (serv.conf.verbose) {
            printf("\n\nSERVER STATE AFTER ITERATION:\n");
            print_server_state(&serv);
        }
    }

    return 0;
}

// Get internet address (sin_addr or sin6_addr) from sockaddr
inline void*
get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (void*) &((struct sockaddr_in*)sa)->sin_addr;
    } else {
        return (void*) &((struct sockaddr_in6*)sa)->sin6_addr;
    }
}

inline unsigned short
get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return ((struct sockaddr_in*)sa)->sin_port;
    } else {
        return ((struct sockaddr_in6*)sa)->sin6_port;
    }
}

// Calls socket(), setsockopt() and then bind().
// Returns socket file descriptor on success, -1 on error.
int
sockbind(struct addrinfo *ai)
{
    int s = socket(ai->ai_family,    // AF_INET or AF_INET6
                   ai->ai_socktype,  // SOCK_STREAM
                   ai->ai_protocol); // 0
    if (s == -1) {
        perror("socket() inside sockbind()");
        return -1;
    }

    int status;

    // Reuse socket address
    int reuse = 1;
    status = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (status == -1) {
        perror("setsockopt() inside sockbind()");
        return -1;
    }

    status = bind(s, ai->ai_addr, ai->ai_addrlen);
    if (status == -1) {
        perror("bind() inside sockbind()");
        close(s);
        return -1;
    }

    return s;
}

// Return number of bytes sent or -1 on error
int
send_buf(int sock, char *buf, size_t len)
{
    size_t nbytes_sent;
    int try = 5;

    // TODO: This retry loop might be unnecessary.
    //
    // * Retrying immediately on EINTR/EAGAIN will most likely
    // do nothing as not enough time will have passed for the
    // socket to become writable.
    //
    // * Run a load test on the server using this retry
    // mechanism and then run one where try=1.
    //
    // My theory is that the latter variant will be slightly
    // faster as it won't unsuccessfully call send() 4 more
    // times every time we have a temporarily unavailable
    // socket and instead will serve other requests and thus
    // pass time until the blocked sockets become available
    // again.
    // 
    // A retry mechanism in general is useful for
    // slow/intermittent connections, so even if I end up
    // removing the one below, it's still wise to have one
    // outside this function.
    for (nbytes_sent = 0; nbytes_sent < len;) {
        int sent = send(sock, buf, len, 0);
        int saved_errno = errno;
        if (sent == -1) {
            switch (saved_errno) {
            case EINTR:
            case EAGAIN:
                if (--try <= 0) {
                    return nbytes_sent;
                }
                continue;
            case ECONNRESET:
            default:
                //perror("send()");
                return -1;
                break;
            }
        } else if (sent == 0) {
            return nbytes_sent;
        } else {
            nbytes_sent += sent;
        }
    }

    return nbytes_sent;
}

// TODO: use sendfile() if available
// TODO: reuse the fd on the file struct
int
send_file(int sock, File *f, size_t len)
{
    return -1;
}

void
hex_dump_line(FILE *stream, char *buf, size_t buf_size, size_t width)
{
    for (size_t i = 0; i < buf_size; i++) {
        switch (buf[i]) {
        case '\n':
            fprintf(stream, "\\n");
            break;
        case '\t':
            fprintf(stream, "\\t");
            break;
        case '\r':
            fprintf(stream, "\\r");
            break;
        default:
            putc(buf[i], stream);
            putc(' ', stream);
            break;
        }
    }

    // Pad the ascii with spaces to fill width
    for (size_t i = buf_size; i < width; i++) {
        putc(' ', stream);
        putc(' ', stream);
    }

    // Hexdump
    fprintf(stream, " | ");
    for (size_t i = 0; i < buf_size; i++) {
        fprintf(stream, "%02X ", (unsigned char) buf[i]);
    }
    putc('\n', stream);
}

void
ascii_dump_buf(FILE *stream, char *buf, size_t buf_size)
{
    for (size_t i = 0; i < buf_size; i++) {
        switch (buf[i]) {
        case '\n':
            fprintf(stream, "\\n\n");
            break;
        case '\t':
            fprintf(stream, "\\t");
            break;
        case '\r':
            fprintf(stream, "\\r");
            break;
        default:
            putc(buf[i], stream);
            break;
        }
    }
}

void
dump_data(FILE *stream, char *buf, size_t buf_size, size_t line_width)
{
    if (buf_size == 0) {
        fprintf(stream, "No data to dump!\n");
        return;
    }

#if HEXDUMP_DATA == 1
    for (size_t i = 0; i < buf_size; i += line_width) {
        hex_dump_line(stream, buf + i, MIN(buf_size - i, line_width),
                      line_width);
    }
#else
    ascii_dump_buf(stream, buf, buf_size, line_width);
#endif
}
