/*
  http server
  Usage: ./mimino [dir/file] [port]
  Serves specified directory 'dir' on 'port'.
  Symlinks to files outside 'dir' are allowed.
  Symlinks to directories outside 'dir' are forbidden.
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

int sockbind(struct addrinfo *ai);
int send_buf(int sock, char *buf, size_t nbytes);
int send_str(int sock, char *buf);
void *get_in_addr(struct sockaddr *sa);
unsigned short get_in_port(struct sockaddr *sa);
void hex_dump_line(FILE *stream, char *buf, size_t buf_size, size_t width);
void dump_data(FILE *stream, char *buf, size_t bufsize, size_t nbytes_recvd);
int find_string_2bufs(char *buf1, char *buf2, char *pat);
void ascii_dump_buf(FILE *stream, char *buf, size_t buf_size);

// Append n bytes from src to buffer's data, growing it if necessary.
// Return 0 on fail.
// Return 1 on success.
int
buf_append(Buffer *b, char *src, size_t n)
{
    if (b->n_items + n > b->n_alloc) {
        char *ptr = realloc(b->data, b->n_alloc + BUFFER_GROWTH);
        if (!ptr) return 0;
        b->data = ptr;
    }
    memcpy(b->data + b->n_items, src, n);
    b->n_items += n;
    return 1;
}

// Append strlen(src) bytes from src to buffer's data, growing it if necessary.
// Return 0 on fail.
// Return 1 on success.
int
buf_append_str(Buffer *b, char *src)
{
    return buf_append(b, src, strlen(src));
}

// Free all parts of a Buffer
void
buf_free(Buffer *b)
{
    free(b->data);
}

/* NOTE: this is a stub while conn is static and has static buffers */
void
free_connection(Connection *conn)
{
    free(conn->req);
    return;
    free(conn->req_buf);
    free(conn->res_buf);
    free(conn);
}

inline Connection
create_connection(int fd, struct pollfd *pfd)
{
    return (Connection) {
        .fd = fd,
        .pollfd = pfd,
        .status = CONN_STATUS_READING,
        .req_buf_i = 0,
        .res_buf_i = 0,
        .read_tries_left = 5,
        .write_tries_left = 5,
    };
}

static Poll_Queue poll_queue;

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
            ipv4_addrinfos = realloc(ipv4_addrinfos,
                                     (ipv4_addrinfos_i + 1) * sizeof(ipv4_addrinfos));
            ipv4_addrinfos[ipv4_addrinfos_i] = rp;
            ipv4_addrinfos_i++;
            break;
        case AF_INET6:
            ipv6_addrinfos = realloc(ipv6_addrinfos,
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
    printf("Got connection from %s:%d\n", their_ip_str,
           ntohs(get_in_port((struct sockaddr *)&their_addr)));

    return newsock;
}

// Returns 1 if read finished successfully
// Returns 0 otherwise (even if an incomplete read happenned)
int
read_request(Connection *conn)
{
    int n = recv(conn->fd, conn->req_buf + conn->req_buf_i,
                 sizeof(conn->req_buf) - conn->req_buf_i, 0);

    if (n < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
            if (conn->read_tries_left == 0) {
                fprintf(stderr, "Reached max read tries for conn\n");
                conn->status = CONN_STATUS_CLOSING;
                conn->pollfd->events = 0;
                return 0;
            }
            conn->read_tries_left--;
        } else { // Some other ESOMETHING error
            perror("recv()");
            conn->status = CONN_STATUS_CLOSING;
            conn->pollfd->events = 0;
        }
        return 0;
    }

    dump_data(stdout, conn->req_buf + conn->req_buf_i, n, DUMP_WIDTH);

    // NOTE: The "\r\n\r\n" might be too limited
    if (n == 0 || strstr(conn->req_buf, "\r\n\r\n")) {
        // Finished reading
        conn->status = CONN_STATUS_WRITING;
        conn->pollfd->events = POLLOUT;
        return 1;
    }

    conn->req_buf_i += n;
    return 0;
}

int
file_list_to_html(File_List *fl, Buffer *buf)
{
    int succ = buf_append_str(
        buf,
        "<!DOCTYPE html><html>"
        "<head><style>"
        "* { font-family: monospace; }\n"
        "table { border: none; margin: 1rem; }\n"
        "td { padding-right: 2rem; }\n"
        "</style></head>"
        "<body><table>\n");
    if (!succ)
        return 0;

    for (size_t i = 0; i < fl->len; i++) {
        // TODO: replace with buf_sprintf sometime later

        // Write file name
        succ &= buf_append_str(buf, "<tr><td>");
        succ &= buf_append_str(buf, fl->files[i].name);
        succ &= buf_append_str(buf, get_file_type_suffix(fl->files + i));

        // Write file size
        succ &= buf_append_str(buf, "</td><td>");
        if (!fl->files[i].is_dir) {
            char *tmp = get_human_file_size(fl->files[i].size);
            succ &= (tmp != NULL);
            succ &= buf_append_str(buf, tmp);
            free(tmp);
        }
        succ &= buf_append_str(buf, "</td><td>\n");

        // Write file permissions
        char *tmp = get_human_file_perms(fl->files + i);
        succ &= (tmp != NULL);
        succ &= buf_append_str(buf, tmp);
        free(tmp);

        if (!succ)
            return 0;
    }

    if (!buf_append_str(buf, "</table></body></html>\r\n"))
        return 0;

    return succ;
}

int
write_response(Server *serv, Connection *conn)
{
    int success = 0;

    // Get file list
    File_List *fl = ls(serv->serve_dir);
    if (!fl)
        goto cleanup;

    Buffer res = {
        .data = malloc(RES_BUF_SIZE),
        .n_items = 0,
        .n_alloc = RES_BUF_SIZE,
    };
    if (!res.data)
        goto cleanup;

    // Write first line into res
    if (!buf_append_str(&res, "HTTP/1.1 200\r\n\r\n"))
        goto cleanup;

    // Write html into res
    if (!file_list_to_html(fl, &res))
        goto cleanup;

    if (!buf_append_str(&res, "\r\n"))
        goto cleanup;

    // Send res
    success = send_buf(conn->fd, res.data, res.n_items);

    // TODO: implement code below
    // file_list = scandir("./" + conn->req->path);
    // html = file_list_to_html(filelist);
    // conn->res_buf = make_http_response(status, headers, html);
    // send(conn->fd, conn->res_buf);

cleanup:
    buf_free(&res);
    free(fl);

    conn->status = CONN_STATUS_CLOSING;
    conn->pollfd->events = 0;
    return success;
}

/*
  TODO: think of a better way to close a connection by passing an actual
  Connection struct.
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

    // Copy last pollfd onto current pollfd
    if (i != pq->pollfd_count - 1)
        pq->pollfds[i] = pq->pollfds[pq->pollfd_count - 1];

    pq->pollfd_count--;
}

int
main(int argc, char **argv)
{
    // Set dir/file path to serve
    char *path = "./";
    if (argc >= 2) {
        path = argv[1];
    }

    // Set port
    char *port = "8080";
    if (argc >= 3) {
        port = argv[2];
    }

    // TODO: fill with data here later
    Server serv = {0};
    serv.serve_dir = path;
    serv.port = port;

    // Init server
    char ip_str[INET6_ADDRSTRLEN];
    struct addrinfo server_addrinfo = {0};
    int listen_sock = init_server(port, &server_addrinfo);
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
    printf("Bound to %s:%s\n", ip_str, port);

    // Start listening
    int backlog = 10;
    if (listen(listen_sock, backlog) == -1) {
        perror("listen()");
        return 1;
    }

    // Init poll_queue
    poll_queue.pollfds[0] = (struct pollfd) {
        .fd = listen_sock,
        .events = POLLIN,
        .revents = 0,
    };
    poll_queue.pollfd_count = 1;

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

        // Accept new connection
        // NOTE: pollfds[0].fd is the listen_sock
        if (poll_queue.pollfds[0].revents & POLLIN) {
            int newsock = accept_new_conn(listen_sock);
            if (newsock != -1) {
                // Update poll_queue
                poll_queue.pollfd_count++;
                int i = poll_queue.pollfd_count - 1;
                // TODO: check bounds for the arrays
                poll_queue.pollfds[i] = (struct pollfd) {
                    .fd = newsock,
                    .events = POLLIN,
                    .revents = 0,
                };
                poll_queue.conns[i] =
                    create_connection(newsock, &poll_queue.pollfds[i]);

                // Restart loop to prioritize new connections
                continue;
            }
        }

        nfds_t open_conn_count = poll_queue.pollfd_count;

        // Iterate poll_queue (starts from 1, skipping listen_sock)
        for (nfds_t fd_i = 1; fd_i < open_conn_count; fd_i++) {
            struct pollfd *pfd = &poll_queue.pollfds[fd_i];
            Connection *conn = &poll_queue.conns[fd_i];
            switch (poll_queue.conns[fd_i].status) {
            case CONN_STATUS_READING:
                if (pfd->revents & POLLIN) {
                    int finished_reading = read_request(conn);
                    if (finished_reading) {
                        Http_Request *req = parse_http_request(conn->req_buf);

                        // Close on invalid request
                        if (req->error) {
                            conn->status = CONN_STATUS_CLOSING;
                            conn->pollfd->events = 0;
                            close_connection(&poll_queue, fd_i);
                            free_http_request(req);
                        }

                        conn->req = req;
                        print_http_request(stdout, conn->req);
                    }
                }
                break;
            case CONN_STATUS_WRITING:
                if (pfd->revents & POLLOUT) {
                    write_response(&serv, conn);
                    // TODO: think of a better way to close the conn here
                    close_connection(&poll_queue, fd_i);
                }
                break;
            case CONN_STATUS_WAITING:
                fprintf(stdout, "CONN_STATUS_WAITING not implemented\n");
            case CONN_STATUS_CLOSING:
                close_connection(&poll_queue, fd_i);
                break;
            case CONN_STATUS_CLOSED:
                break;
            }
        }

        /*char file_buf[16];
        size_t file_buf_len = sizeof(file_buf) * sizeof(char);

        // Open file
        FILE *file_handle = fopen(path, "r");
        if (file_handle == NULL) {
            perror("fopen()");
            goto close;
        }

        // Start reading the file and sending it.
        // This works without loading the entire file into memory.
        while (1) {
            size_t bytes_read = fread(file_buf, 1, file_buf_len, file_handle);
            if (bytes_read == 0) {
                if (ferror(file_handle)) {
                    fprintf(stdout, "Error when fread()ing file %s\n", path);
                }
                break;
            }

            int succ = send_buf(newsock, file_buf, bytes_read);
            if (!succ) {
                goto close;
            }

            if (bytes_read < file_buf_len) {
                // fread() will return 0, so if we break here, we avoid calling it
                break;
            }
        };

        fclose(file_handle);
        fprintf(stdout, "Finished fread()ing\n"); */
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

int
send_buf(int sock, char *buf, size_t len)
{
    for (size_t nbytes_sent = 0; nbytes_sent < len;) {
        int sent = send(sock, buf, len, 0);
        int saved_errno = errno;
        if (sent == -1) {
            perror("send()");
            switch (saved_errno) {
            case EINTR:
            case EAGAIN:
                continue;
            case ECONNRESET:
            default:
                return 0;
                break;
            }
        } else if (sent == 0) {
            return 0;
        } else {
            nbytes_sent += sent;
        }
    }

    return 1;
}

inline int
send_str(int sock, char *buf)
{
    return send_buf(sock, buf, strlen(buf));
}

// Searches for pat in buf1 concatenated with buf2.
int
find_string_2bufs(char *buf1, char *buf2, char *pat)
{
    // TODO: do this without malloc
    size_t l1 = strlen(buf1), l2 = strlen(buf2);
    char *b = malloc(l1 + l2);
    memcpy(b, buf1, l1);
    memcpy(b + l1 + 1, buf2, l2);
    int ret = (strstr(b, pat) != NULL);
    free(b);
    return ret;
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
        fprintf(stream, "%02X ", buf[i]);
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
    if (buf_size == 0)
        return;
#if HEXDUMP_DATA == 1
    for (size_t i = 0; i < buf_size; i += line_width) {
        hex_dump_line(stream, buf + i, MIN(buf_size - i, line_width),
                      line_width);
    }
#else
    ascii_dump_buf(stream, buf, buf_size, line_width);
#endif
}
