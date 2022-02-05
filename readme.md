# mimino

WIP http server for educational purposes.

## Usage
- `mimino [filename]` serves a single file.
- `mimino [dirname]` serves a directory.

## TODO
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
