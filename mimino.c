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
void *get_in_addr(struct sockaddr *sa);
unsigned short get_in_port(struct sockaddr *sa);
void hex_dump_line(FILE *stream, char *buf, size_t buf_size, size_t width);
void dump_data(FILE *stream, char *buf, size_t bufsize, size_t nbytes_recvd);
void ascii_dump_buf(FILE *stream, char *buf, size_t buf_size);

int
buf_grow(Buffer *b, size_t min_growth)
{
    size_t new_size = b->n_alloc + MAX(min_growth, BUFFER_GROWTH);
    char *ptr = realloc(b->data, new_size);
    if (!ptr) return 0;
    b->n_alloc = new_size;
    b->data = ptr;
    return 1;
}

// Append n bytes from src to buffer's data, growing it if necessary.
// Return 0 on fail.
// Return 1 on success.
int
buf_append(Buffer *b, char *src, size_t n)
{
    if (n == 0) return 1;

    if (b->n_items + n > b->n_alloc) {
        if (!buf_grow(b, n)) return 0;
    }

    memcpy(b->data + b->n_items, src, n);
    b->n_items += n;
    return 1;
}

// Push one byte into buffer's data, growing it if necessary.
// Return 0 on fail.
// Return 1 on success.
int
buf_push(Buffer *b, char c)
{
    if (b->n_items + 1 > b->n_alloc) {
        if (!buf_grow(b, 1)) return 0;
    }
    b->data[b->n_items] = c;
    b->n_items++;
    return 1;
}

// Does not copy the null terminator
int
buf_append_str(Buffer *b, char *src)
{
    return buf_append(b, src, strlen(src));
}

int
buf_append_href(Buffer *buf, File *f, char *req_path)
{
    int succ = 1;

    // dirname
    succ &= buf_append_str(buf, req_path);
    if (req_path[strlen(req_path) - 1] != '/')
        succ &= buf_push(buf, '/');

    // basename
    succ &= buf_append_str(buf, f->name);

    // trailing '/' if given file is a directory
    if (f->is_dir) {
        succ &= buf_push(buf, '/');
    }

    return succ;
}

// Does not copy the null terminator
int
buf_sprintf(Buffer *buf, char *fmt, ...)
{
    va_list fmtargs;
    size_t len;

    // Determine formatted length
    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);
    len++;

    // Grow buffer if necessary
    if (buf->n_items + len > buf->n_alloc)
        if (!buf_grow(buf, len)) return 0;

    va_start(fmtargs, fmt);
    vsnprintf(buf->data + buf->n_items, len, fmt, fmtargs);
    va_end(fmtargs);

    // Exclude the null terminator at the end
    buf->n_items--;

    return 1;
}

// Return -1 on fopen error
// Return 0 on read error
// Return 1 on success
int
buf_append_file_contents(Buffer *buf, File *f, char *path)
{
    if (buf->n_items + f->size > buf->n_alloc) {
        buf_grow(buf, f->size);
    }

    FILE *file_handle = fopen(path, "r");
    if (!file_handle) {
        perror("fopen()");
        return -1;
    }
    
    while (1) {
        size_t bytes_read = fread(buf->data + buf->n_items, 1, f->size, file_handle);
        buf->n_items += bytes_read;
        if (bytes_read < (size_t) f->size) {
            if (ferror(file_handle)) {
                fprintf(stdout, "Error when freading() file %s\n", path);
                fclose(file_handle);
                return 0;
            }
            // EOF
            fclose(file_handle);
            return 1;
        }
    }

    fclose(file_handle);
    return 1;
}

// Free all parts of a Buffer
void
free_buf(Buffer *b)
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
    int n = recv(conn->fd, conn->req_buf + conn->req_buf_i,
                 sizeof(conn->req_buf) - conn->req_buf_i, 0);

    if (n < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
            if (conn->read_tries_left == 0) {
                fprintf(stderr, "Reached max read tries for conn\n");
                return -1;
            }
            conn->read_tries_left--;
        } else { // Some other ESOMETHING error
            perror("recv()");
            return -1;
        }
        return 0;
    }

    //dump_data(stdout, conn->req_buf + conn->req_buf_i, n, DUMP_WIDTH);

    // NOTE: The "\r\n\r\n" might be too limited
    if (n == 0 || strstr(conn->req_buf, "\r\n\r\n")) {
        // Finished reading
        return 1;
    }

    conn->req_buf_i += n;
    return 0;
}

// req_path is the path inside the HTTP request
int
file_list_to_html(char *req_path, File_List *fl, Buffer *buf)
{
    int succ = buf_append_str(
        buf,
        "<!DOCTYPE html><html>"
        "<head><style>"
        "* { font-family: monospace; }\n"
        "table { border: none; margin: 1rem; }\n"
        "td { padding-right: 2rem; }\n"
        ".red { color: crimson; }\n"
        "</style></head>"
        "<body><table>\n");
    if (!succ)
        return 0;

    for (size_t i = 0; i < fl->len; i++) {
        File *f = fl->files + i;

        // Write file name
        succ &= buf_append_str(buf, "<tr><td><a href=\"");
        succ &= buf_append_href(buf, f, req_path);
        succ &= buf_push(buf, '"');
        if (f->is_link && f->is_broken_link)
            succ &= buf_append_str(buf, " class=\"red\"");
        succ &= buf_push(buf, '>');
        succ &= buf_append_str(buf, f->name);
        succ &= buf_append_str(buf, get_file_type_suffix(f));

        // Write file size
        succ &= buf_append_str(buf, "</a></td><td>");
        if (!f->is_dir) {
            char *tmp = get_human_file_size(f->size);
            succ &= (tmp != NULL);
            succ &= buf_append_str(buf, tmp);
            free(tmp);
        }
        succ &= buf_append_str(buf, "</td><td>\n");

        // Write file permissions
        char *tmp = get_human_file_perms(f);
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

// TODO: make write_response work with poll() loop so that it can resume sending where it left off.
// TODO: clean up the gotos
// Return 1 if done writing completely.
// Return 0 if an incomplete read happened.
// Return -1 on fatal error or max retry reached.
int
write_response(Server *serv, Connection *conn)
{
    int result = -1;

    char *path = resolve_path(serv->serve_path, conn->req->path);

    printf("serve path: %s\n", serv->serve_path);
    printf("request path: %s\n", conn->req->path);
    printf("resolved path: %s\n", path);

    // TODO: if path is higher than serve_path, return 403

    Buffer res = {
        .data = malloc(RES_BUF_SIZE),
        .n_items = 0,
        .n_alloc = RES_BUF_SIZE,
    };
    if (!res.data) goto cleanup;

    // Find out if we're listing a dir or serving a file
    char *base_name = get_base_name(path);
    if (!base_name) goto cleanup;
    File file;
    int read_result = read_file_info(&file, path, base_name);
    free(base_name);
    if (read_result == -2) {
        // Fatal error
        if (!buf_append_str(&res, "HTTP/1.1 500\r\n\r\n"))
            goto cleanup;
    } else if (read_result == -1) {
        // File not found
        if (!buf_append_str(&res, "HTTP/1.1 404\r\n\r\n"))
            goto cleanup;
        // TODO: buf_append_http_error_msg(404);
        if (!buf_append_str(&res, "Error 404: file not found\r\n"))
            goto cleanup;
    } else {
        if (!buf_append_str(&res, "HTTP/1.1 200\r\n\r\n"))
            goto cleanup;

        if (file.is_dir) {
            // Get file list
            File_List *fl = ls(path);
            if (!fl) goto cleanup;

            // Write html into res
            if (!file_list_to_html(conn->req->path, fl, &res)) {
                free_file_list(fl);
                free(fl);
                goto cleanup;
            }
            free_file_list(fl);
            free(fl);
        } else {
            int succ = buf_append_file_contents(&res, &file, path);
            if (succ == -1 || succ == 0)
                goto cleanup;
        }

        if (!buf_append_str(&res, "\r\n"))
            goto cleanup;
    }

    // Send res
    result = send_buf(conn->fd, res.data, res.n_items);

cleanup:
    if (path != serv->serve_path)
        free(path);
    free_buf(&res);
    free_file(&file);

    return result;
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
    pq->pollfds[i].events = 0;

    // Copy last pollfd and conn onto current pollfd and conn
    if (i != pq->pollfd_count - 1) {
        pq->pollfds[i] = pq->pollfds[pq->pollfd_count - 1];
        pq->conns[i] = pq->conns[pq->pollfd_count - 1];
    }

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
    serv.serve_path = path;
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

    // Add listen_sock to poll_queue it
    poll_queue.pollfds[0] = (struct pollfd) {
        .fd = listen_sock,
        .events = POLLIN,
        .revents = 0,
    };
    poll_queue.pollfd_count = 1;
    // NOTE: This connection won't be used; it's the listen socket
    poll_queue.conns[0] = create_connection(listen_sock,
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

        // Accept new connection
        // pollfds[0].fd is the listen_sock
        if (poll_queue.pollfds[0].revents & POLLIN) {
            int newsock = accept_new_conn(listen_sock);
            if (newsock != -1) {
                // Update poll_queue
                poll_queue.pollfd_count++;
                int i = poll_queue.pollfd_count - 1;
                /*
                  TODO: check bounds for the arrays. If not fixed,
                  ths will blow up.

                  Also, make sure to grow conns array in tandem
                  with pollfds array.
                 */
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
            Connection *conn = &poll_queue.conns[fd_i];

            switch (conn->status) {
            case CONN_STATUS_READING:
                if (conn->pollfd->revents & POLLIN) {
                    int status = read_request(conn);
                    printf("read_request status %i\n", status);
                    if (status == -1) {
                        // Reading failed
                        close_connection(&poll_queue, fd_i);
                    } else if (status == 1) {
                        // Reading done, parse request
                        Http_Request *req = parse_http_request(conn->req_buf);
                        print_http_request(stdout, req);

                        // Close on parse error
                        if (req->error) {
                            fprintf(stdout, "Parse error: %s\n", req->error);
                            close_connection(&poll_queue, fd_i);
                        }

                        // Start writing
                        conn->req = req;
                        conn->status = CONN_STATUS_WRITING;
                        conn->pollfd->events = POLLOUT;
                    }
                }
                break;
            case CONN_STATUS_WRITING:
                if (conn->pollfd->revents & POLLOUT) {
                    int status = write_response(&serv, conn);
                    printf("write_response status %i\n", status);
                    if (status == 1) {
                        // Close when done.
                        // Even HTTP errors like 5xx or 4xx go here.
                        free_http_request(conn->req);
                        close_connection(&poll_queue, fd_i);
                    } else if (status == -1) {
                        // Fatal error encountered and can't send data.
                        // TODO: use conn->res->error instead
                        //       (After creating Http_Response struct)
                        if (conn->req->error) {
                            fprintf(stdout,
                                    "Error when writing response: %s\n",
                                    conn->req->error);
                            free_http_request(conn->req);
                            close_connection(&poll_queue, fd_i);
                        }
                    }
                }
                break;
            case CONN_STATUS_WAITING:
                fprintf(stdout, "CONN_STATUS_WAITING not implemented\n");
                break;
            case CONN_STATUS_CLOSED:
                close_connection(&poll_queue, fd_i);
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

// Return 1 on success
// Return 0 on nothing sent
// Return -1 on error
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
                // FIXME: This can loop forever
                continue;
            case ECONNRESET:
            default:
                return -1;
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
