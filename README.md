# mimino

WIP http server for educational purposes.

Needs to be "faster" than nginx before April 1st.

## Usage
`mimino [filename]` serves a single file.
`mimino [dirname]` serves a directory.

## TODO
- read RFCs and find out which version's better
    - [list of http rfcs in this wikipedia page](https://en.wikipedia.org/wiki/Hypertext_Transfer_Protocol)
    - [how to read an rfc](https://www.ietf.org/blog/how-read-rfc/)
    - [ctrl-f hypertext](https://www.rfc-editor.org/standards)
    - [HTTP 1.0 RFC](https://datatracker.ietf.org/doc/html/rfc1945)
    - [HTTP 1.1 RFC](https://datatracker.ietf.org/doc/html/rfc2616)
    - [other internet standard RFCs](https://www.rfc-editor.org/search/rfc_search_detail.php?sortkey=Number&sorting=DESC&page=All&pubstatus%5B%5D=Standards%20Track&std_trk=Internet%20Standard)
    - [Roy Fieldings REST dissertation](https://www.ics.uci.edu/~fielding/pubs/dissertation/top.htm)
- Read this: https://www.khanacademy.org/computing/computers-and-internet/xcae6f4a7ff015e7d:the-internet/xcae6f4a7ff015e7d:web-protocols/a/hypertext-transfer-protocol-http
- Right now I'm send()ing stuff bit by bit. Need to find out how big an HTTP request can get
- GoodSocket by jart: https://github.com/jart/cosmopolitan/blob/master/libc/sock/goodsocket.c
- read https://developer.mozilla.org/en-US/docs/Web/HTTP
- HTTP protocol
- configuration support (in json?)
  - fastCGI (or some other CGI thing)
  - HTTPs (for SSL cert paths)
