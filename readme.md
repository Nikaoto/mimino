# mimino

## Features (WIP)

- Dirlisting
- HEAD requests
- Range/partial requests
- Connection keep-alive and timeouts
- Single-threaded using poll()
- Supports both ipv4 and ipv6

## Man page
_WIP_

After finished, the man page should look like this:

```
NAME
    mimino - Quickly serve a directory or static website

SYNOPSIS
    mimino [-vqure46] [-p PORT] [-P HTTPS_PORT] [-i [INDEXFILE]]
           [-s [SUFFIX]] [FILE/DIRECTORY]

DESCRIPTION
    Mimino is a zero-configuration, simple and small web server
    ideal for hosting a home server. Supports both IP v4 and v6.
    Does not support SSL. See mimino-forwarder for SSL support.

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
          '.html'. Multiple suffixes can be given like so:
          
              mimino -s .htm,.html
          
          will first check '.htm' files, then '.html' ones.

    -p PORT
          Sets port to listen to for HTTP connections. Default
          is 8080.

    -i INDEXFILE
          Mimino will search for INDEXFILE inside the directory
          it is serving. If INDEXFILE is not provided, but this
          flag is set, it will default to 'index.html'. If this
          flag is not set, Mimino will serve a listing of the
          directory. Multiple index files can be given like so:
          
              mimino -i index.html,index.htm,index.cgi

AUTHOR
    Written by Nikoloz Otiashvili.
```


## License (3-clause BSD)
Copyright 2022 Nikoloz Otiashvili.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
