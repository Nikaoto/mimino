## TODO

- handle more than 20 simultaneous connections
- `ag TODO:`
- add unit tests
  - for `parse_http_request`
  - for `buf_encode_url`
  - for `cleanup_path`
  - for `resolve_path`

- security
  - time out connections that don't send/receive data for 20 seconds
  - something about changing the GID and UID of the UNIX-domain socket file
  - restrict linking to directories outside serve_dir (unless `serv.conf.unsafe`
    is set)

- features
  - mobile css
  - `-d` flag for defaults (?)
    Equivalent to `-e -iindex.html,index.htm,index -s.html,.htm -p80`
  - table for mime types
  - make connections and pollfds arrays dynamic
  - `If-Modified-Since`
  - Support Range / partial content for streaming or resuming a download
    - discard requests with invalid byte ranges
    - code 206
    - read about If-Range header
  - Use sendfile() when possible
  - support ipv6 (just start listen()ing on one ipv6 socket)
  - add flags mentioned in ./readme.md
  - think of a way to enable both dirlisting and indexing (maybe `-d` flag to
  disable dirlistings?)


- optimizaiton
  - use hashmap instead of lookup table for mime types
  - use hashmap to parse headers instead of if-else chain
  - when serving a single non-directory file, don't resolve any paths
  - caching (with infinite (?) TTL)
  - use readdir() instead of scandir() for faster dir scanning
    (~/src/darkhttpd/darkhttpd.c:1830:0)
  - Try on improvements from GoodSocket by jart:
  https://github.com/jart/cosmopolitan/blob/master/libc/sock/goodsocket.c and
  measure actual performance increase

- Reading
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
  - [darkhttpd](https://unix4lyfe.org/darkhttpd/)

### Logging
With logging, I want to find out:
 1. how many requests I am serving per day, per week, per month.
 2. how many bytes I am receiving/sending per day, per week, per month
 3. how many unique visitors I am getting per day, per week, per month
 4. how many concurrent connections I am handling right now (and how many are
    in which state), something like nginx_status
 5. average request duration in last 5 minutes or so
 6. total memory usage of mimino process

- For numbers 1 and 3: Log requests and analyze the logs using a sed script.
- For numbers 4, 5: Have a statsfile that gets updated every 10 seconds by mimino.
- For number 6: Use the statsfile and get VmSize and VmRSS values from
  /proc/self/status (https://stackoverflow.com/a/64166)

#### Log Format

**Date format in logs**
```
2022-05-10T14:51:38+04:00
```

**Normal request logs**
```
date IP "method url httpversion" httpcode referer useragent
```

**Error logs**

Only the issues that are most likely caused by the server are considered errors.
For example, if a client stops receiving data, that's not an error, just a debug
log saying we couldn't send any more data.

Error logs (actual errors like 5xx, not 4xx) should appear along with the normal
request log for the request that caused the error:
```
ERR-0001: Normal request log
ERR-0001: Error message, which
ERR-0001: can be multiline.
ERR-0002: Normal request log
ERR-0002: This is a different error with a different id.
```

This way, all (even multiline) errors can be searched with `grep logfile ^ERR`.
The number is just the ID of the error, used only to distinguish between
different error messages.

**Debug logs**
```
DEB: msg
```

## Pre-release checklist

- Run performance test with https://github.com/wg/wrk
- Write mimino-forwarder
- Remove unnecessary logging & printfs
- Test download with 9GB file
- Test paused download with 9GB file
- Test video streaming

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
