# mimino

After finished, the man page should look like this (perhaps without SSL?):

```
NAME
    mimino - Quickly serve a directory or static website

SYNOPSIS
    mimino [-vqure] [-p PORT] [-P HTTPS_PORT] [-i INDEXFILE]
           [-c CERTFILE -k KEYFILE] [FILE/DIRECTORY]

DESCRIPTION
    Mimino is a zero-configuration, simple and small web server
    ideal for hosting a home server. Supports SSL.

OPTIONS
    -v    Verbose mode, log more.

    -q    Quiet mode, disable all logging.

    -u    Unsafe mode. Allows symlinks to directories outside the
          served directory. By default this is disabled and only
          symlinks to non-directory files are allowed.

    -r    chroot(3) to the directory on startup. Disables -u.

    -e    Search for standard 'xxx.html' error files in case of
          errors (404.html, 500.html...)

    -s SUFFIX
          If this flag is set, Mimino will append SUFFIX to every
          requested URL and serve the file if found. For example,
          if it's set to '.txt', when clients request the path
          '/hello', it will serve the contents of the file at
          '/hello.txt' if found, otherwise it will serve the
          contents of the file '/hello'. If SUFFIX is not
          provided, but this flag is set, it will default to
          '.html'.

    -p PORT
          Sets port to listen to for HTTP connections. Default
          is 8080.

    -P HTTPS_PORT
          Sets port to listen to for HTTPS connections. Default
          is 443. Uses CERTFILE and KEYFILE for connections on
          this port, so -c and -k are implied.

    -i INDEXFILE
          Mimino will search for INDEXFILE inside the directory
          it is serving. If INDEXFILE is not provided, but this
          flag is set, it will default to 'index.html'. If this
          flag is not set, Mimino will serve a listing of the
          directory.

    -c CERTFILE
          Path to PEM-format X.509 certificate file.
          Implies -k and -P.

    -k KEYFILE
          Path to PEM-format X.509 key file.
          Implies -c and -P.

AUTHOR
    Written by Nikoloz Otiashvili.
```
