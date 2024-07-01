/*
  Use getifaddrs(3) to print interface addresses like 'os.networkInterfaces()'
  in nodejs.
*/

#include <errno.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <string.h>
#include <arpa/inet.h>

#define GET_SIN_ADDR(sa)                          \
    ((sa)->sa_family == AF_INET ? \
        (void *)&(((struct sockaddr_in*)(sa))->sin_addr) :   \
        (void *)&(((struct sockaddr_in6*)(sa))->sin6_addr))
        
#define GET_SOCKLEN(sa)                 \
    ((sa->sa_family == AF_INET6) ?      \
        (sizeof(struct sockaddr_in6)) : \
        (sizeof(struct sockaddr_in)))

int
main(void)
{
    struct ifaddrs *ifap;
    if (getifaddrs(&ifap) != 0) {
        fprintf(stderr, "getifaddrs() failed: %s\n", strerror(errno));
        return 1;
    }

    for (struct ifaddrs *i = ifap; i; i = i->ifa_next) {
        printf("(struct ifaddrs) {\n");

        // ifa_next
        printf("  ifa_next = ");
        if (!i->ifa_next) {
            printf("(null)");
        } else {
            printf("{...}");
        }
        printf(",\n");

        // ifa_name
        printf("  ifa_name = \"%s\",\n", i->ifa_name);

        // ifa_flags
        printf("  ifa_flags = 0x%.8X,\n", i->ifa_flags);

        // ifa_addr
        if (i->ifa_addr &&
            (i->ifa_addr->sa_family == AF_INET ||
             i->ifa_addr->sa_family == AF_INET6)) {
            char addr_str[INET6_ADDRSTRLEN];
            char *succ = inet_ntop(
                i->ifa_addr->sa_family,
                GET_SIN_ADDR(i->ifa_addr),
                addr_str,
                (socklen_t) INET6_ADDRSTRLEN);
            if (!succ) {
                fprintf(stderr,
                        "inet_ntop() failed on ifa_addr: %s\n", strerror(errno));
                return 1;
            }
            printf("  ifa_addr = \"%s\",\n", addr_str);
        } else {
            printf("  ifa_addr = (null),\n");
        }

        // ifa_netmask
        if (i->ifa_netmask &&
            (i->ifa_netmask->sa_family == AF_INET ||
             i->ifa_netmask->sa_family == AF_INET6)) {
            char netmask_str[INET6_ADDRSTRLEN];
            char *succ = inet_ntop(
                i->ifa_netmask->sa_family,
                GET_SIN_ADDR(i->ifa_netmask),
                netmask_str,
                (socklen_t) INET6_ADDRSTRLEN);
            if (!succ) {
                fprintf(stderr,
                        "inet_ntop() failed on ifa_netmask: %s\n", strerror(errno));
                return 1;
            }
            printf("  ifa_netmask = \"%s\",\n", netmask_str);
        } else {
            printf("  ifa_netmask = (null),\n");
        }

        // ifa_broadaddr / ifa_dstaddr
        if (IFF_BROADCAST & i->ifa_flags) {
            if(i->ifa_broadaddr &&
               (i->ifa_broadaddr->sa_family == AF_INET ||
                i->ifa_broadaddr->sa_family == AF_INET6)) {
                char broad[INET6_ADDRSTRLEN];
                char *succ = inet_ntop(
                    i->ifa_broadaddr->sa_family,
                    GET_SIN_ADDR(i->ifa_broadaddr),
                    broad,
                    (socklen_t) INET6_ADDRSTRLEN);
                if (!succ) {
                    fprintf(stderr,
                            "inet_ntop() failed on ifa_broadaddr: %s\n", strerror(errno));
                    return 1;
                }
                printf("  ifa_broadaddr = \"%s\",\n", broad);
            } else {
                printf("  ifa_broadaddr = (null),\n");
            }
        } else if (IFF_POINTOPOINT & i->ifa_flags) {
            if(i->ifa_dstaddr &&
               (i->ifa_dstaddr->sa_family == AF_INET ||
                i->ifa_dstaddr->sa_family == AF_INET6)) {

                char dst[INET6_ADDRSTRLEN];
                char *succ = inet_ntop(
                    i->ifa_dstaddr->sa_family,
                    GET_SIN_ADDR(i->ifa_dstaddr),
                    dst,
                    (socklen_t) INET6_ADDRSTRLEN);
                if (!succ) {
                    fprintf(stderr,
                            "inet_ntop() failed on ifa_dstaddr: %s\n", strerror(errno));
                    return 1;
                }
                printf("  ifa_dstaddr = \"%s\",\n", dst);
            } else {
                printf("  ifa_dstaddr = (null),\n");
            }
        }

        // ifa_data
        printf("  ifa_data = ");
        if (i->ifa_data) {
            printf("Ox....,\n");
        } else {
            printf("(null),\n");
        }
        printf("}\n\n");
    }

    freeifaddrs(ifap);
    return 0;
}
