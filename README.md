# mimino

WIP http server for educational purposes.

Needs to be "faster" than nginx before April 1st.

## Usage
`mimino [filename]` serves a single file.
`mimino [dirname]` serves a directory.

## TODO
- GoodSocket by jart: https://github.com/jart/cosmopolitan/blob/master/libc/sock/goodsocket.c
- read https://developer.mozilla.org/en-US/docs/Web/HTTP
- HTTP protocol
- configuration support (in json?)
  - fastCGI (or some other CGI thing)
  - HTTPs (for SSL cert paths)

