/*
  Print address info.
  Usage: ./addrinfo [addr] [port]
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

int main(int argc, char **argv)
{
    // Set remote address
    char *url = "www.example.com";
    if (argc >= 2) {
        if (strncmp(argv[1], "_", 1) == 0)
            url = NULL;
        else
            url = argv[1];
    }
    
    // Set port
    char *port = "80";
    if (argc >= 3) {
        port = argv[2];
    }

    struct addrinfo hints = {
        .ai_flags = AI_PASSIVE | (url ? AI_CANONNAME : 0x0),
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
        0
    };
    struct addrinfo *getaddrinfo_res;

    // Get server info
    int err = getaddrinfo(url, port, &hints, &getaddrinfo_res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(err));
        return 1;
    }

    // Print server info (for each ai_next)
    for (struct addrinfo *rp = getaddrinfo_res; rp != NULL; rp = rp->ai_next) {
        char ip_str[INET6_ADDRSTRLEN];
        unsigned short port_int = 80;

        if (rp->ai_family == AF_INET) {
            // Write ipv4 into buffer
            const char *ok;
            ok = inet_ntop(AF_INET,
                           &(((struct sockaddr_in*)(rp->ai_addr))->sin_addr),
                           ip_str,
                           INET_ADDRSTRLEN);
            if (ok == NULL) {
                perror("inet_ntop");
            }
            // Get port
            port_int = ntohs(((struct sockaddr_in*)(rp->ai_addr))->sin_port);
        } else if (rp->ai_family == AF_INET6) {
            // Write ipv6 into buffer
            const char *ok;
            ok = inet_ntop(AF_INET6,
                           &(((struct sockaddr_in6*) (rp->ai_addr))->sin6_addr),
                           ip_str,
                           INET6_ADDRSTRLEN);
            if (ok == NULL) {
                perror("inet_ntop");
            }
            // Get port
            port_int = ntohs(((struct sockaddr_in6*)(rp->ai_addr))->sin6_port);
        } else {
            fprintf(stderr, "Unknown AF (addrinfo family): %d\n", rp->ai_family);
            continue;
        }

        printf("'%s', ip: %s, port: %u, canonname:'%s'\n", url, ip_str, port_int, rp->ai_canonname);
    }

    freeaddrinfo(getaddrinfo_res);
    return 0;
}
