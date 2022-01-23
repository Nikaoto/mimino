/*
  http server
  Usage: ./mimino [port] [dir/file]
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
  bind(sockfd, res->ai_addr, res->ai_addrlen);
  listen(sockfd, backlog);
  while (newsock = accept(sockfd, &their_addr, their_addr_size))) {
    recv(newsock, buf, buflen, 0);
    send(newsock, msg, msglen, 0);
    close(newsock);
  }
*/

#define HEXDUMP_DATA 1
#define BUFSIZE 10

int sockbind(struct addrinfo *ai);
void *get_in_addr(struct sockaddr *sa);
unsigned short get_in_port(struct sockaddr *sa);
void print_recvd_data(FILE *stream, char *buf, size_t bufsize, size_t nbytes_recvd);
int find_string_2bufs(char *buf1, char *buf2, char *pat);

int
main(int argc, char **argv)
{
    // Set port
    char *port = "8080";
    if (argc >= 2) {
        port = argv[1];
    }

    // Set file to send
    char *file_name = "README.md";
    /* size_t file_name_len = 1; */
    /* if (argc >= 3) { */
    /*     file_name = argv[2]; */
    /* } */
    /* file_name_len = strlen(file_name); */

    int backlog = 10;
    int sockfd, saved_errno;
    char ip_str[INET6_ADDRSTRLEN];
    char their_ip_str[INET6_ADDRSTRLEN];
    struct addrinfo *getaddrinfo_res;
    struct addrinfo *chosen_addrinfo = NULL;
    struct addrinfo **ipv4_addrinfos = NULL;
    int ipv4_addrinfos_i = 0;
    struct addrinfo **ipv6_addrinfos = NULL;
    int ipv6_addrinfos_i = 0;

    // Init hints
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Get server info
    int err = getaddrinfo(NULL, port, &hints, &getaddrinfo_res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(err));
        return 1;
    }

    // Populate ipv4_addrinfos and ipv6_addrinfos arrays
    for (struct addrinfo *rp = getaddrinfo_res; rp != NULL; rp = rp->ai_next) {
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

    // Try binding to ipv4 (prefer ipv4 over ipv6)
    for (int i = 0; i < ipv4_addrinfos_i; i++) {
        sockfd = sockbind(ipv4_addrinfos[i]);
        if (sockfd != -1) {
            chosen_addrinfo = malloc(sizeof(*chosen_addrinfo));
            memcpy(chosen_addrinfo, ipv4_addrinfos[i], sizeof(*chosen_addrinfo));
            break;
        }
    }

    // Try binding to ipv6
    if (chosen_addrinfo == NULL) {
        for (int i = 0; i < ipv6_addrinfos_i; i++) {
            sockfd = sockbind(ipv6_addrinfos[i]);
            if (sockfd != -1) {
                chosen_addrinfo = malloc(sizeof(*chosen_addrinfo));
                memcpy(chosen_addrinfo, ipv6_addrinfos[i], sizeof(*chosen_addrinfo));
                break;
            }
        }
    }

    free(ipv4_addrinfos);
    free(ipv6_addrinfos);
    freeaddrinfo(getaddrinfo_res);

    // Exit if failed to bind
    if (chosen_addrinfo == NULL) {
        fprintf(stderr, "Could not bind to any address.");
        return 1;
    }

    // Get ip_str. Exit if chosen ip is munged
    if (inet_ntop(chosen_addrinfo->ai_family,
                  get_in_addr(chosen_addrinfo->ai_addr),
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

    // Listen
    err = listen(sockfd, backlog);
    if (err != 0) {
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
    struct sockaddr_storage their_addr;
    socklen_t their_addr_size = sizeof(their_addr);
    int newsock;
    while (1) {
        // Accept new connection
        newsock = accept(sockfd, (struct sockaddr *)&their_addr, &their_addr_size);
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
                    drop_connection = 1;
                    perror("recv()");
                    break;
                default:
                    perror("recv()");
                    return 1;
                }
            } else if (bytes_recvd == 0) {
                printf("recv() returned 0\n");
                break;
            } else {
                // Print data
                print_recvd_data(stdout, buf, BUFSIZE, bytes_recvd);
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
            fprintf(stderr, "Client refused connection, dropping\n");
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

        // Start reading and sending it
        while (1) {
            size_t bytes_read = fread(file_buf, 1, file_buf_len, file_handle);
            if (bytes_read == 0) {
                if (ferror(file_handle)) {
                    fprintf(stdout, "Error when fread()ing file %s\n", file_name);
                }
                break;
            }

            for (size_t bytes_sent = 0; bytes_sent < bytes_read;) {
                int sent = send(newsock, file_buf, bytes_read, 0);
                saved_errno = errno;
                if (sent == -1) {
                    perror("send()");
                    switch (saved_errno) {
                    case EINTR:
                    case EAGAIN:
                        continue;
                    default:
                        return 1;
                    }
                } else if (sent == 0) {
                    goto close;
                } else {
                    bytes_sent += sent;
                }
            }

            if (bytes_read < file_buf_len) {
                // fread() will return 0, so if we break here, we avoid calling it
                break;
            }
        };

        fclose(file_handle);
        fprintf(stdout, "Finished fread()ing\n");

        // Send the HTTP end
        char http_end[4] = {'\r', '\n', '\r', '\n' };
        size_t http_end_size = sizeof(http_end) * sizeof(char);
        send(newsock, http_end, http_end_size, 0);
        /* for (size_t bytes_sent = 0; bytes_sent < http_end_size;) { */
        /*     int sent = send(newsock, http_end + bytes_sent, http_end_size, 0); */
        /*     saved_errno = errno; */
        /*     if (sent == -1) { */
        /*         perror("send()"); */
        /*         switch (saved_errno) { */
        /*         case EINTR: */
        /*         case EAGAIN: */
        /*             continue; */
        /*         default: */
        /*             return 1; */
        /*         } */
        /*     } else if (sent == 0) { */
        /*         goto close; */
        /*     } else { */
        /*         bytes_sent += sent; */
        /*     } */
        /* } */
        fprintf(stdout, "Finished send()ing\n");

        // Close newsock
      close:
        for (int closed = 0; closed == 0;) {
            err = close(newsock);
            saved_errno = errno;
            if (err != 0) {
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
void*
get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (void*) &((struct sockaddr_in*)sa)->sin_addr;
    } else {
        return (void*) &((struct sockaddr_in6*)sa)->sin6_addr;
    }
}

unsigned short
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

// Searches for pat in buf1 concatenated with buf2.
int find_string_2bufs(char *buf1, char *buf2, char *pat)
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
print_recvd_data(FILE *stream, char *buf, size_t bufsize, size_t nbytes_recvd)
{
#if HEXDUMP_DATA == 1
    for (size_t i = 0; i < nbytes_recvd; i++) {
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

    // Pad data with spaces
    for (size_t i = nbytes_recvd; i < bufsize; i++) {
        putc(' ', stream);
        putc(' ', stream);
    }

    // Hexdump data
    fprintf(stream, " | ");
    for (size_t i = 0; i < nbytes_recvd; i++) {
        fprintf(stream, "%02X ", buf[i]);
    }
    putc('\n', stream);
#else // #if HEXDUMP_DATA == 1
    for (size_t i = 0; i < nbytes_recvd; i++) {
        putc(buf[i], stream);
    }
#endif // #if HEXDUMP_DATA == 1
}
