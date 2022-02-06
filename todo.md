## TODO
- restrict linking to directories outside serve_dir
- allow linking to files outside serve_dir
- serve index.html if present
- add flag -p for port
- add flag -c for chroot to serve_dir at init
- add flag -e to search for standard xxx.html error files in case of errors (404.html, 500.html ...)
- add flag -i for index.html (if arg is empty, default to index.html)
- rename `free_*` functions to `free_*_parts`
- use sendfile() whenever possible
- Support Range / partial content for streaming or resuming a download
- Support HEAD requests
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
- move (almost) all structs and header contents to mimino.h
- Right now I'm send()ing stuff bit by bit. Need to find out how big an HTTP request can get
- GoodSocket by jart: https://github.com/jart/cosmopolitan/blob/master/libc/sock/goodsocket.c
- read https://developer.mozilla.org/en-US/docs/Web/HTTP
- read https://unix4lyfe.org/darkhttpd/

## Pre-release checklist
- Run performance test with https://github.com/wg/wrk
- Use epoll on linux, kqueue on bsd and osx, poll otherwise
- Write mimino-forwarder

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

### stunnel (or own implementation?)
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
