#include "mimino.h"
#include "connection.h"
#include "http.h"

Connection
make_connection(int fd, Server *s, nfds_t i)
{
    return (Connection) {
        .fd = fd,
        .pollfd = s->queue.pollfds + i,
        .status = CONN_STATUS_READING,
        .req = NULL,
        .res = NULL,
        .read_tries_left = 5,
        .write_tries_left = 5,
        .keep_alive = 1,
        .last_active = s->time_now,
    };
}

void
free_connection_parts(Connection *conn)
{
    if (!conn) return;

    free_http_request(conn->req);
    free_http_response(conn->res);
    //free(conn);
}

void
print_connection(struct pollfd *pfd, Connection *conn)
{
    if (!conn) {
        printf("(Connection) NULL\n");
        return;
    }

    printf("(struct pollfd) {\n");
    printf("  .fd = %d,\n", pfd->fd);
    printf("  .events = %0X,\n", pfd->events);
    printf("  .revents = %0X,\n", pfd->revents);
    printf("}\n");

    printf("(Connection) {\n");
    printf("  .status = %i,\n", conn->status);
    printf("  .read_tries_left = %d,\n", conn->read_tries_left);
    printf("  .write_tries_left = %d,\n", conn->write_tries_left);
    printf("  .keep_alive = %d,\n", conn->keep_alive);
    printf("  .last_active = %ld,\n", (long) conn->last_active);
    printf("  .req = ");
    print_http_request(stdout, conn->req);
    printf("  .res = \n");
    print_http_response(stdout, conn->res);
    printf("}\n");
}
