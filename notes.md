# Notes on unix networking

## Server pseudocode
```
res = getaddrinfo(NULL, port, &hints, &res);
sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
bind(sockfd, res->ai_addr, res->ai_addrlen);
listen(sockfd, backlog);
while (newsock = accept(sockfd, &their_addr, their_addr_size))) {
  recv(newsock, buf, buflen, 0);
  send(newsock, msg, msglen, 0);
  close(newsock);
}
```

Hints for getaddrinfo:
```
hints = {
  .ai_flags = AI_PASSIVE,     // Fill in my ip for me
  .ai_family = AF_UNSPEC,     // Return ipv4 or ipv6 addr
  .ai_socktype = SOCK_STREAM, // Use TCP
  .ai_protocol = 0,           // Automatically assign
};
```

## Client pseudocode
```
res = getaddrinfo("www.example.com", port, &hints, &res);
sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
// bind() is optional here as connect() will do it for us
connect(sockfd, res->ai_addr, res->ai_addrlen);
send(sockfd, msg, msglen, 0);
close(sockfd);
```

Hints for getaddrinfo:
```
hints = {
  .ai_flags = AI_CANONNAME,   // Fill in the remote host's .ai_canonname
  .ai_family = AF_UNSPEC,     // Return ipv4 or ipv6 addr
  .ai_socktype = SOCK_STREAM, // Use TCP
  .ai_protocol = 0,           // Automatically assign
};
```

## struct `addrinfo`
Used for getaddrinfo hints and returned by getaddrinfo():
```
struct addrinfo
{
  int              ai_flags;     // Input flags.
  int              ai_family;    // Protocol family for socket.
  int              ai_socktype;  // Socket type.
  int              ai_protocol;  // Protocol for socket.
  socklen_t        ai_addrlen;   // Length of socket address.
  struct sockaddr *ai_addr;      // Socket address.
  char            *ai_canonname; // Canonical name for remote host.
  struct addrinfo *ai_next;      // Pointer to next addrinfo in list.
};
```

* `ai` stands for addrinfo
* `ai_flags` - `AI_CANONNAME`/`AI_PASSIVE`
* `ai_family` - `AF_UNSPEC`/`AF_INET`/`AF_INET6`
* `ai_socktype` - `SOCK_STREAM`/`SOCK_DGRAM`/`SOCK_RAW`
* `ai_protocol` - `0`

## struct `sockaddr`
```
struct sockaddr
{
  unsigned short sa_family;
  char           sa_data[14];
};
```

## struct `sockaddr_in`
Can be cast to sockaddr. Holds address of ipv4 socket.
```
struct sockaddr_in
{
  short int          sin_family;
  unsigned short int sin_port;
  struct in_addr     sin_addr;    // Described below
  unsigned char      sin_zero[8];
}

struct in_addr
{
  uint32_t s_addr;
}
```

## struct `sockaddr_in6`
Holds address of ipv6 socket.
```
struct sockaddr_in6
{
  u_int16_t       sin6_family;
  u_int16_t       sin6_port;
  u_int16_t       sin6_flowinfo;
  struct in6_addr sin6_addr;      // Described below
  u_int32_t       sin6_scope_id;
}

struct in6_addr
{
  unsigned char s6_addr[16];
}
```

## struct `sockaddr_storage`
Can be cast to `sockaddr`, `sockaddr_in` or `sockaddr_in6`. Fits all three.
This exists because `sockaddr_in6` (26 bytes) is too big to fit into `sockaddr` (16 bytes).
```
struct sockaddr_storage
{
  sa_family_t ss_family; // AF_INET or AF_INET6
  
  // This padding is implementation specific, unimportant
  char __ss_pad1[_SS_PAD1SIZE];
  int64_t __ss_align;
  char __ss_pad2[_SS_PAD2SIZE];
}
```
