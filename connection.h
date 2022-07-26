#ifndef _MIMINO_CONNECTION_H
#define _MIMINO_CONNECTION_H

#include <poll.h>
#include "mimino.h"

Connection make_connection(int fd, struct pollfd *pfd, time_t t);
void free_connection_parts(Connection *conn);
void print_connection(struct pollfd *pfd, Connection *conn);

#endif // _MIMINO_CONNECTION_H
