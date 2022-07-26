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

static Poll_Queue poll_queue;

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
    printf("}\n");
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

// Return 1 if done reading completely.
// Return 0 if an incomplete read happened.
// Return -1 on fatal error or max retry reached.
int
read_request(Connection *conn)
{
    // TODO: instead of having a separate raw request string for
    // each request, a static one and discard it after parsing the
    // request (we can also parse it exactly here after reading it
    // fully)
    int n = recv(conn->fd,
                 conn->req->buf->data + conn->req->buf->n_items,
                 conn->req->buf->n_alloc - conn->req->buf->n_items,
                 0);
    if (n < 0) {
        int saved_errno = errno;
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
            if (conn->read_tries_left == 0) {
                fprintf(stderr, "Reached max read tries for conn\n");
                return -1;
            }
            conn->read_tries_left--;
        }
        errno = saved_errno;
        perror("recv()");
        return 0;
    }

    //dump_data(stdout,
    //          conn->req->buf->data + conn->req->buf->n_items,
    //          n,
    //          DUMP_WIDTH);

    conn->req->buf->n_items += n;

    // Finished reading
    if (n == 0 || is_http_end(conn->req->buf->data, conn->req->buf->n_items)) {
        return 1;
    }

    // TODO: handle case when request is too large - send 413
    // if (conn->req->buf->n_items == conn->req->buf->n_alloc)

    return 0;
}

// Return 1 if done writing completely.
// Return 0 if an incomplete write happened.
// Return -1 on fatal error or max retry reached.
int
write_response(Server *serv, Connection *conn)
{
    int sent = send_buf(
        conn->fd,
        conn->res->buf->data + conn->res->nbytes_sent,
        conn->res->buf->n_items);

    // Fatal error, no use retrying
    if (sent == -1) {
        conn->res->error = "Other error";
        return -1;
    }

    conn->res->nbytes_sent += sent;

    // Retry later if not sent fully
    if (conn->res->nbytes_sent < conn->res->buf->n_items) {
        if (conn->write_tries_left == 0) {
            conn->res->error = "Max write tries reached";
            return -1;
        }
        conn->write_tries_left--;

        return 0;
    }

    // Fully sent
    //dump_data(stdout, res.data, res.n_items, DUMP_WIDTH);
    return 1;
}

/*
  TODO: think of a better way to close a connection by passing an actual
  Connection struct or a Server struct.
*/
void
close_connection(Poll_Queue *pq, nfds_t i)
{
    /*
      No need to check for errno here as most sane OSes close the file
      descriptor early in their close syscall. So, retrying with the same fd
      might close some other file. For more info read close(2) manual.
    */
    if (close(pq->pollfds[i].fd) == -1) {
        perror("close()");
    }

    pq->conns[i].status = CONN_STATUS_CLOSED;
    pq->pollfds[i].events = 0;

    // Copy last pollfd and conn onto current pollfd and conn
    if (i != pq->pollfd_count - 1) {
        pq->pollfds[i] = pq->pollfds[pq->pollfd_count - 1];
        pq->conns[i] = pq->conns[pq->pollfd_count - 1];
        pq->conns[i].pollfd = pq->pollfds + i;
    }

    pq->pollfd_count--;
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
    };

    // Set chroot directory
    if (serv.conf.chroot && serv.conf.chroot_dir == NULL) {
        serv.conf.chroot_dir = serv.conf.serve_path;
    }

    // Print server configuration
    if (!serv.conf.quiet) print_server_config(&serv.conf);
    
    // Init server
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

    // Add listen_sock to poll_queue
    poll_queue.pollfds[0] = (struct pollfd) {
        .fd = listen_sock,
        .events = POLLIN,
        .revents = 0,
    };
    poll_queue.pollfd_count = 1;
    // NOTE: This connection won't be used; it's the listen socket
    poll_queue.conns[0] = make_connection(listen_sock,
                                          &poll_queue.pollfds[0]);

    // Main loop
    while (1) {
        int nfds = poll(poll_queue.pollfds, poll_queue.pollfd_count, -1);
        if (nfds == -1) {
            perror("poll() returned -1");
            return 1;
        }

        if (nfds == 0) {
            fprintf(stderr, "poll() returned 0\n");
            continue;
        }

        if (serv.conf.verbose) {
            printf("\n\nSERVER STATE BEFORE ITERATION:\n");
            printf("pollfd_count: %zu\n", poll_queue.pollfd_count);
            for (nfds_t i = 1; i < poll_queue.pollfd_count; i++) {
                printf("Connection %zu:\n", i);
                print_connection(poll_queue.pollfds + i,
                                 poll_queue.conns + i);
                printf("------------\n");
            }
        }

        // Accept new connection
        // pollfds[0].fd is the listen_sock
        if (poll_queue.pollfds[0].revents & POLLIN) {
            if (poll_queue.pollfd_count >= MAX_CONN) {
                fprintf(
                    stderr,
                    "poll queue reached maximum capacity of %d\n",
                    MAX_CONN);
            } else {
                int newsock = accept_new_conn(listen_sock);
                if (newsock != -1) {
                    // Update poll_queue
                    poll_queue.pollfd_count++;
                    int i = poll_queue.pollfd_count - 1;
                    poll_queue.pollfds[i] = (struct pollfd) {
                        .fd = newsock,
                        .events = POLLIN,
                        .revents = 0,
                    };
                    poll_queue.conns[i] =
                        make_connection(newsock, poll_queue.pollfds + i);

                    // Restart loop to prioritize new connections
                    continue;
                }
            }
        }

        // Iterate poll_queue (starts from 1, skipping listen_sock)
        for (nfds_t fd_i = 1; fd_i < poll_queue.pollfd_count; fd_i++) {
            Connection *conn = &poll_queue.conns[fd_i];

            switch (conn->status) {
            case CONN_STATUS_READING: {
                if (!(conn->pollfd->revents & POLLIN))
                    continue;

                // Allocate memory for request struct
                if (conn->req == NULL) {
                    conn->req = xmalloc(sizeof(*(conn->req)));
                    memset(conn->req, 0, sizeof(*(conn->req)));
                    // We won't grow this buffer
                    conn->req->buf = new_buf(MAX_REQUEST_SIZE);
                }

                int status = read_request(conn);
                if (status == -1) {
                    // Reading failed
                    free_connection_parts(conn);
                    close_connection(&poll_queue, fd_i);
                } else if (status == 1) {
                    // Reading done, parse request
                    parse_http_request(conn->req);
                    //print_http_request(stderr, req);

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
                            free_connection_parts(conn);
                            close_connection(&poll_queue, fd_i);
                        }
                    }

                    // Set keep-alive
                    if (!strcasecmp(conn->req->connection, "close")) {
                        conn->keep_alive = 0;
                    }

                    // Start writing
                    conn->status = CONN_STATUS_WRITING;
                    poll_queue.pollfds[fd_i].events = POLLOUT;
                }
                break;
            }
            case CONN_STATUS_WRITING: {
                if (!(conn->pollfd->revents & POLLOUT))
                    continue;

                if (!conn->res) {
                    conn->res = make_http_response(&serv, conn->req);

                    if (serv.conf.verbose) {
                        printf("-----------------\n");
                        print_http_request(stdout, conn->req);
                        print_http_response(stdout, conn->res);
                        printf("-----------------\n");
                    }
                }

                int status = write_response(&serv, conn);
                if (status == 1) {
                    if (!conn->keep_alive) {
                        // Close when done.
                        // Even HTTP errors like 5xx or 4xx go here.
                        free_connection_parts(conn);
                        close_connection(&poll_queue, fd_i);
                    } else {
                        // TODO: Recycle connection here
                        //   * zero the connection
                        //   * set its status to CONN_READING
                        free_connection_parts(conn);
                        close_connection(&poll_queue, fd_i);
                    }
                } else if (status == -1) {
                    // Fatal error, can't send data.
                    if (conn->res->error) {
                        fprintf(stdout,
                                "Error when writing response: %s\n",
                                conn->req->error);
                        free_connection_parts(conn);
                        close_connection(&poll_queue, fd_i);
                    }
                }
                break;
            }
            case CONN_STATUS_WAITING:
                fprintf(stderr, "CONN_STATUS_WAITING not implemented\n");
                break;
            case CONN_STATUS_CLOSED:
                fprintf(stderr, "WARN: connection not closed\n");
                close_connection(&poll_queue, fd_i);
                break;
            }
        }

        if (serv.conf.verbose) {
            printf("\n\nSERVER STATE AFTER ITERATION:\n");
            printf("pollfd_count: %zu\n", poll_queue.pollfd_count);
            for (nfds_t i = 1; i < poll_queue.pollfd_count; i++) {
                printf("Connection %zu:\n", i);
                print_connection(poll_queue.pollfds + i,
                                 poll_queue.conns + i);
                printf("------------\n");
            }
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

    for (nbytes_sent = 0; nbytes_sent < len;) {
        int sent = send(sock, buf, len, 0);
        int saved_errno = errno;
        if (sent == -1) {
            perror("send()");
            switch (saved_errno) {
            case EINTR:
            case EAGAIN:
                if (--try <= 0) {
                    return nbytes_sent;
                }
                continue;
            case ECONNRESET:
            default:
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
