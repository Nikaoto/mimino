# mimino

After finished, the man page should look like this:

```
NAME
    mimino - Quickly serve a directory or static website

SYNOPSIS
    mimino [-vqure] [-p PORT] [-S HTTPS_PORT] [-i INDEXFILE]
           [-c CERTFILE -k KEYFILE] [FILE/DIRECTORY]

DESCRIPTION
    Mimino is a zero-configuration, simple and small web server
    ideal for hosting a home server. Supports SSL.

OPTIONS
    -v    Verbose mode, log more.

    -q    Quiet mode, disable all logging.

    -u    Unsafe mode. Allows symlinks to directories outside the
          served directory. By default this is disabled, only
          symlinks to non-directory files are allowed.

    -r    chroot(3) to the directory on startup. Disables -u.

    -e    Search for standard 'xxx.html' error files in case of
          errors (404.html, 500.html...)

    -p PORT
          Default is 8080.

    -S HTTPS_PORT
          Default is 443. Uses CERTFILE and KEYFILE for
          connections on this port.

    -i INDEXFILE
          If this flag is set, Mimino will
          search for INDEXFILE inside the directory it is
          serving. If INDEXFILE is not provided, it will default
          to 'index.html'. If this flag is not set, Mimino will
          serve a listing of the directory.

    -c CERTFILE
          Path to PEM-format X.509 certificate file.
          Implies -k and -S.

    -k KEYFILE
          Path to PEM-format X.509 key file.
          Implies -c and -S.

AUTHOR
    Written by Nikoloz Otiashvili.
```

## TODO
- rename `free_*` functions to `free_*_parts`
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
- Do SSL (pass certs to cli)
- move (almost) all structs and header contents to mimino.h
- Right now I'm send()ing stuff bit by bit. Need to find out how big an HTTP request can get
- GoodSocket by jart: https://github.com/jart/cosmopolitan/blob/master/libc/sock/goodsocket.c
- read https://developer.mozilla.org/en-US/docs/Web/HTTP
