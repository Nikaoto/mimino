## TODO
- add unit tests
  - for `parse_args`
  - for `decode_url`
  - for `parse_http_request`
  - for `buf_encode_url`
  - for `cleanup_path`
  - for `resolve_path`

- security
  - timeout for lingering connections
  - something about changing the GID and UID of the UNIX-domain socket file
  - restrict linking to directories outside serve_dir (unless `serv.conf.unsafe` is set)

- logging / debugging
  - log the server configuration when starting up
  - put a verbose flag check before every log

- features
  - table for mime types
  - move Poll_Queue struct inside Server struct and make connections and pollfds arrays dynamic (maybe write some tests before doing rewrites like this)
  - Support Range / partial content for streaming or resuming a download
  - Use sendfile() when possible
  - Support HEAD requests(?)
  - support ipv6 (just start listen()ing on one ipv6 socket)
  - read about keep-alive. Is it worth implementing?
  - add flags mentioned in ./readme.md

- optimizaiton
  - use hashmap instead of lookup table for mime types
  - when serving a single non-directory file, don't resolve any paths
  - caching with infinite (?) TTL
  - use readdir() instead of scandir() for faster dir scanning (~/src/darkhttpd/darkhttpd.c:1830:0)

- Skim RFCs
  - [HTTP/1.1 - Syntax & Routing (main)](https://datatracker.ietf.org/doc/html/rfc7230)
  - [HTTP/1.1 - Status Codes, Methods & Headers ](https://datatracker.ietf.org/doc/html/rfc7231)
  - [HTTP/1.1 - Authentication](https://datatracker.ietf.org/doc/html/rfc7235)
  - [HTTP/1.1 - ETags & Last-Modified](https://datatracker.ietf.org/doc/html/rfc7232)
  - [HTTP/1.1 - Caching](https://datatracker.ietf.org/doc/html/rfc7234)
  - [HTTP/1.1 - Partial Responses](https://datatracker.ietf.org/doc/html/rfc7233)
  - [list of http rfcs in this wikipedia page](https://en.wikipedia.org/wiki/Hypertext_Transfer_Protocol)
  - [HTTP 1.0 RFC](https://datatracker.ietf.org/doc/html/rfc1945)
  - [old HTTP 1.1 RFC](https://datatracker.ietf.org/doc/html/rfc2616)
  - [other internet standard RFCs](https://www.rfc-editor.org/search/rfc_search_detail.php?sortkey=Number&sorting=DESC&page=All&pubstatus%5B%5D=Standards%20Track&std_trk=Internet%20Standard)
  - [Roy Fieldings REST dissertation](https://www.ics.uci.edu/~fielding/pubs/dissertation/top.htm)
- Try on improvements from GoodSocket by jart:
  https://github.com/jart/cosmopolitan/blob/master/libc/sock/goodsocket.c and
  measure actual performance increase
- read https://developer.mozilla.org/en-US/docs/Web/HTTP
- read https://unix4lyfe.org/darkhttpd/

## Pre-release checklist
- Run performance test with https://github.com/wg/wrk
- Write mimino-forwarder
- remove unnecessary logging & printfs

## Ideas

### mimino-forwarder
Imagine the following:
```
                                      ,-> (mimino -p8081 ~/nikaoto.com/)
 (client) <--> (mimino-forwarder) <---
                                      `-> (mimino -p8082 ~/rawliverclub.com/)
```
mimino-forwarder takes a host->port mapping from the cli:
```
mimino-forwarder -h nikaoto.com:8081 -h rawliverclub.com:8082
```

### stunnel (or own implementation?) or hitch
Can write a separate tunnel that just builds with openbsd SSL library and set
that in front of each mimino process, so I won't need to have SSL crammed into
mimino itself, keeps it simple.

So an init script for something like:
```
                                      ,-> (stunnel -8001:9001) <--> (mimino -p9001 ~/nikaoto.com/)
 (client) <--> (mimino-forwarder) <---
                                      `-> (stunnel -p8002:9002) <--> (mimino -p9002 ~/rawliverclub.com/)
```

Should look like:
```
#! /usr/bin/env bash

mimino-forwarder \
    -h nikaoto.com:8001 \
    -h rawliverclub.com:8002 \
    >> /var/log/mimino-forwarder.log &

# Start serving nikaoto.com w/ tunnel
stunnel -c nikaoto.com-cert.pem -k nikaoto.com-key.pem \
    -p8001:9001 >> /var/log/stunnel-nikaoto.com.log &
mimino -i -p9001 ~/nikaoto.com/ >> /var/log/nikaoto.com.log &

# Start serving rawliverclub.com w/ tunnel
stunnel -c rawliverclub.com-cert.pem -k rawliverclub.com-key.pem \
    -p8002:9002 >> /var/log/stunnel-rawliverlcub.com.log &
mimino -i -p9002 ~/rawliverclub.com/ >> /var/log/rawliverclub.com.log &
```

# Other
Read request with socket emptying:
```
// Return 1 if done reading completely.
// Return 0 if an incomplete read happened.
// Return -1 on fatal error or max retry reached.
int
read_request(Connection *conn)
{
    static char trash_buf[40960];
    int n;
    size_t nbytes_to_recv = conn->req->buf->n_alloc - conn->req->buf->n_items;

    if (nbytes_to_recv == 0) {
        // We've already filled our req buffer, this will empty the socket
        n = recv(conn->fd, trash_buf, sizeof(trash_buf), 0);
    } else {
        n = recv(conn->fd,
                 conn->req->buf->data + conn->req->buf->n_items,
                 nbytes_to_recv,
                 0);
    }

    printf("n: %d\n", n);
    printf("buf->n_alloc: %zu\n", conn->req->buf->n_alloc);
    printf("buf->n_items: %zu\n", conn->req->buf->n_items);
    printf("nbytes_to_recv: %zu\n", nbytes_to_recv);

    if (n < 0) {
        int saved_errno = errno;
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
            if (conn->read_tries_left == 0) {
                fprintf(stderr, "Reached max read tries for conn\n");
                return -1;
            }
            conn->read_tries_left--;
        }
        errno = saved_errno;
        perror("recv()");
        return 0;
    }

    //dump_data(stdout,
    //          conn->req->buf->data + conn->req->buf->n_items,
    //          n,
    //          DUMP_WIDTH);

    if (nbytes_to_recv != 0)
        conn->req->buf->n_items += n;

    // Finished reading (or request buffer full)
    if (n == 0 ||
        conn->req->buf->n_items == conn->req->buf->n_alloc ||
        is_http_end(conn->req->buf->data, conn->req->buf->n_items)) {
        return 1;
    }

    return 0;
}
```
