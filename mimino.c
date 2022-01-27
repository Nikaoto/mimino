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

#ifndef BUFSIZE
#define BUFSIZE 10
#endif

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
main(int argc, char **argv)
{
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
    int sock = init_server(port, &server_addrinfo);
    if (sock == -1) {
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
    if (listen(sock, backlog) == -1) {
        perror("listen()");
        return 1;
    }

    /*
       Main loop:
       - accept() connection
       - recv() data and print it
       - send() data
       - close() connection

       TODO: Main loop with poll()
       - poll() fds in poll_queue for POLLIN
       - accept() connection and add its sockfd to the poll_queue
         Make sure that it doesn't block AND it doesn't loop forever and use up
         CPU if there are no incoming connections
       - for each fd with POLLIN in poll_queue
         - read & print data completely
         -? send() data to fd
         -? if sent completely, poll_queue
         -? if not sent completely, add it to send_queue with remaining data in a buffer
       - poll() fds in send_queue for POLLOUT
    */
    char their_ip_str[INET6_ADDRSTRLEN];
    struct sockaddr_storage their_addr;
    socklen_t their_addr_size = sizeof(their_addr);
    int newsock, saved_errno;
    while (1) {
        // Accept new connection
        newsock = accept(sock, (struct sockaddr *)&their_addr, &their_addr_size);
        saved_errno = errno;
        if (newsock == -1) {
            if (saved_errno != EAGAIN) {
                fprintf(stderr, "%d", saved_errno);
                perror("accept()");
            }
            continue;
        }

        // Don't block on newsock
        if (fcntl(newsock, F_SETFL, O_NONBLOCK) != 0) {
            perror("fcntl()");
            continue;
        }

        // Print their_addr
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  their_ip_str, sizeof(their_ip_str));
        printf("Got connection from %s:%d\n", their_ip_str,
               ntohs(get_in_port((struct sockaddr *)&their_addr)));

        // Receive
        // TODO: use poll() for recv()
        // TODO: merge the send() loop with this recv() loop?
        fprintf(stdout, "recv()ing data:\n");
        int drop_connection = 0, maxtries = 5, request_finished = 0;
        char last_buf[BUFSIZE];
        char buf[BUFSIZE];
        for (int tries = 0; !drop_connection && !request_finished;) {
            int bytes_recvd = recv(newsock, buf, BUFSIZE, 0);
            if (bytes_recvd == -1) {
                saved_errno = errno;

                switch (saved_errno) {
                case EAGAIN:
                    break;
                case EINTR:
                    if (tries++ >= maxtries) {
                        fprintf(stderr, "recv() maxtries\n");
                        drop_connection = 1;
                    }
                    printf("EINTR\n");
                    break;
                case ECONNREFUSED:
                default:
                    drop_connection = 1;
                    perror("recv()");
                    break;
                }
            } else if (bytes_recvd == 0) {
                printf("recv() returned 0\n");
                break;
            } else {
                dump_data(stdout, buf, bytes_recvd, BUFSIZE);
                // Check for HTTP end
                if (find_string_2bufs(last_buf, buf, "\r\n\r\n")) {
                    request_finished = 1;
                    break;
                }
                memcpy(last_buf, buf, BUFSIZE);
            }
        }

        fprintf(stdout, "Finished recv()\n");

        if (drop_connection) {
            fprintf(stderr, "Client refused or error occured, dropping connection\n");
            goto close;
            continue;
        }

        // Send
        // TODO: use poll()
        fprintf(stdout, "send()ing data\n");
        //int file_fd = open(file_name); // TODO: put this in the poll loop as well
        char file_buf[16];
        size_t file_buf_len = sizeof(file_buf) * sizeof(char);

        // Open file
        FILE *file_handle = fopen(file_name, "r");
        if (file_handle == NULL) {
            perror("fopen()");
            goto close;
        }

        // Start reading the file and sending it.
        // This works without loading the entire file into memory.
        int succ = send_str(newsock, "HTTP/1.0 200");
        if (!succ) {
            goto close;
        }

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
        fprintf(stdout, "Finished fread()ing\n");

        // Send the HTTP end
        succ = send_str(newsock, "\r\n\r\n");
        if (!succ) {
            goto close;
        }
        fprintf(stdout, "Finished send()ing\n");

        // Close newsock
      close:
        for (int closed = 0; closed == 0;) {
            if (close(newsock) != 0) {
                saved_errno = errno;
                perror("close()");
                switch (saved_errno) {
                case EBADF:
                case EIO:
                    closed = 1;
                    break;
                case EINTR:
                default:
                    continue;
                }
            } else {
                closed = 1;
            }
        }

        fprintf(stdout, "close()d the socket\n");
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
#if HEXDUMP_DATA == 1
    hex_dump_line(stream, buf, buf_size, line_width);
#else
    ascii_dump_buf(stream, buf, buf_size, line_width);
#endif
}
