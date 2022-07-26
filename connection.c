#include "mimino.h"
#include "connection.h"
#include "http.h"

Connection
make_connection(int fd, struct pollfd *pfd)
{
    return (Connection) {
        .fd = fd,
        .pollfd = pfd,
        .status = CONN_STATUS_READING,
        .req = NULL,
        .res = NULL,
        .read_tries_left = 5,
        .write_tries_left = 5,
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
    printf("  .req = ");
    print_http_request(stdout, conn->req);
    printf("  .res = \n");
    print_http_response(stdout, conn->res);
    printf("}\n");
}
