/*
  http server
  Usage: ./mimino [dir/file] [port]
  Serves specified directory 'dir' on 'port'.
  Symlinks to files outside 'dir' are allowed.
  Symlinks to directories outside 'dir' are forbidden.
*/

#include <stdio.h>
#include <stdlib.h>
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

/*
  getaddrinfo(NULL, port, &hints, &res);
  // iterate and choose preferred res
  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  bind(sockfd, res->ai_addr, res->ai_addrlen);
  listen(sockfd, backlog);
  while (newsock = accept(sockfd, &their_addr, their_addr_size))) {
    recv(newsock, buf, buflen, 0);
    send(newsock, msg, msglen, 0);
    close(newsock);
  }
*/

#ifndef HEXDUMP_DATA
#define HEXDUMP_DATA 0
#endif

#ifndef DUMP_WIDTH
#define DUMP_WIDTH 10
#endif

#define REQ_SIZE 1024
#define RES_SIZE 1024

#define CONN_STATUS_READING   1
#define CONN_STATUS_WRITING   2
#define CONN_STATUS_WAITING   3
#define CONN_STATUS_CLOSING   4
#define CONN_STATUS_CLOSED    5

typedef struct {
    int fd;
    struct pollfd *pollfd;
    // NOTE: both req and res will be changed to dynamic char buffers
    char req[REQ_SIZE];
    size_t req_i;
    char res[RES_SIZE];
    size_t res_i; // Points to char up to which data was sent
    int status;
    int read_tries_left; // read_request tries left until force closing
    int write_tries_left; // write_response tries left until force closing
} Connection;

/* NOTE: this is a stub while conn is static and has static buffers */
void
free_connection(Connection *conn)
{
    return;
    free(conn->req);
    free(conn->res);
    free(conn);
}

Connection
create_connection(int fd, struct pollfd *pfd)
{
    return (Connection) {
        .fd = fd,
        .pollfd = pfd,
        .status = CONN_STATUS_READING,
        .req_i = 0,
        .res_i = 0,
        .read_tries_left = 5,
        .write_tries_left = 5,
    };
}

typedef struct {
    struct pollfd pollfds[20];
    nfds_t pollfd_count;
    Connection conns[21]; // First conn is ignored
} Poll_Queue;

static Poll_Queue poll_queue;

int sockbind(struct addrinfo *ai);
int send_buf(int sock, char *buf, size_t nbytes);
int send_str(int sock, char *buf);
void *get_in_addr(struct sockaddr *sa);
unsigned short get_in_port(struct sockaddr *sa);
void hex_dump_line(FILE *stream, char *buf, size_t buf_size, size_t width);
void dump_data(FILE *stream, char *buf, size_t bufsize, size_t nbytes_recvd);
int find_string_2bufs(char *buf1, char *buf2, char *pat);

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

// Returns 0 on error or close
// Returns 1 if a read happened
int
read_request(Connection *conn)
{
    int n = recv(conn->fd, conn->req + conn->req_i, sizeof(conn->req) - conn->req_i, 0);

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

    dump_data(stdout, conn->req + conn->req_i, n, DUMP_WIDTH);

    if (n == 0 || strstr(conn->req, "\r\n\r\n")) {
        // Finished reading
        conn->status = CONN_STATUS_WRITING;
        conn->pollfd->events = POLLOUT;
        return 1;
    }
}

int
write_response(Connection *conn)
{
    int succ = send_str(conn->fd,
        "HTTP/1.1 200\r\n\r\nGagimarjos\r\n");
    conn->status = CONN_STATUS_CLOSING;
    conn->pollfd->events = 0;

    return succ;
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
dirsort (const void *x, const void *y)
{
    struct dirent *a = *(struct dirent**)x;
    struct dirent *b = *(struct dirent**)y;

    if (!strcmp(a->d_name, "."))
        return -1;
    if (!strcmp(b->d_name, "."))
        return 1;
    if (!strcmp(a->d_name, ".."))
        return -1;
    if (!strcmp(b->d_name, ".."))
        return 1;

    // TODO: do fstat on a and b

    return strcmp(a->d_name, b->d_name);
}

int
main(int argc, char **argv)
{
    /* List dir and sort */
    /* struct dirent **namelist; */
    /* int n = scandir(".", &namelist, NULL, NULL); */
    /* if (n == -1) { */
    /*     perror("scandir()"); */
    /*     return 1; */
    /* } */

    /* qsort((void*) namelist, (size_t) n, sizeof(struct dirent*), dirsort); */

    /* for (int i = 0; i < n; i++) { */
    /*     printf("%s\n", namelist[i]->d_name); */
    /*     free(namelist[i]); */
    /* } */
    /* free(namelist); */

    /* return 0; */

    // Set file to send
    char *file_name = "README.md";
    if (argc >= 2) {
        file_name = argv[1];
    }

    // Set port
    char *port = "8080";
    if (argc >= 3) {
        port = argv[2];
    }

    // Init server
    char ip_str[INET6_ADDRSTRLEN];
    struct addrinfo server_addrinfo = {0};
    int listen_sock = init_server(port, &server_addrinfo);
    if (listen_sock == -1) {
        fprintf(stderr, "init_server() failed.\n");
        return 1;
    }

    // Print server IP. Exit if it's is munged
    if (inet_ntop(server_addrinfo.ai_family,
                  get_in_addr(server_addrinfo.ai_addr),
                  ip_str, sizeof(ip_str)) == NULL) {
        perror("inet_ntop()");
        return 1;
    }
    printf("Bound to %s:%s\n", ip_str, port);

    // Don't block on sockfd
    /* if (fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0) { */
    /*     perror("fcntl()"); */
    /*     return 1; */
    /* } */

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
                // TODO: check bounds for the arrays
                poll_queue.pollfd_count++;
                int i = poll_queue.pollfd_count - 1;
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
                    fprintf(stdout, "recv()ing data:\n");
                    read_request(conn);
                    fprintf(stdout, "Finished recv()\n");
                    // TODO: parse_http_request(conn->req)
                    // and based on Content-Length maybe keep reading.
                    // TODO: also, close if invalid request
                }
                break;
            case CONN_STATUS_WRITING:
                if (pfd->revents & POLLOUT) {
                    fprintf(stdout, "send()ing data\n");
                    fprintf(stdout, "%d\n", write_response(conn));
                    fprintf(stdout, "Finished send()ing\n");
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
        FILE *file_handle = fopen(file_name, "r");
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
                    fprintf(stdout, "Error when fread()ing file %s\n", file_name);
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

// TODO: incorporate this into the poll() loop
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
ascii_dump_buf(FILE *stream, char *buf, size_t buf_size, size_t delim_width)
{
    /* char delim = '-'; */

    /* for (size_t i = 0; i < delim_width; i++) { */
    /*     putc(delim, stream); */
    /* } */
    /* putc('\n', stream); */

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

    /* putc('\n', stream); */
    /* for (size_t i = 0; i < delim_width; i++) { */
    /*     putc(delim, stream); */
    /* } */
    /* putc('\n', stream); */
}

void
dump_data(FILE *stream, char *buf, size_t buf_size, size_t line_width)
{
    if (buf_size == 0)
        return;
#if HEXDUMP_DATA == 1
    hex_dump_line(stream, buf, buf_size, line_width);
#else
    ascii_dump_buf(stream, buf, buf_size, line_width);
#endif
}
