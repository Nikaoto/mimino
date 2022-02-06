# mimino

After finished, the man page should look like this (perhaps without SSL?):

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
