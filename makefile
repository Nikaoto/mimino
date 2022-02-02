# Makefile
CC := cc
CWARNS := -Wall -Wextra -Wno-comments -Wno-implicit-fallthrough
CFLAGS := -pipe -O $(CWARNS) -g $(DEFS)
FAST_CFLAGS := -pipe -O2 -Wall -Wpedantic -Wextra -g $(DEFS)
LINK := $(CC)

all: mimino

# fast: mimino.c
# 	$(CC) $(FAST_CFLAGS) mimino.c -o $@

mimino: mimino.o http.o
	$(CC) $^ $(CFLAGS) -o $@

%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@

recvserver: recvserver/recvserver.c
	$(CC) $@/*.c $(CFLAGS) -o $@/$@

addrinfo: addrinfo/addrinfo.c
	$(CC) $@/*.c $(CFLAGS) -o $@/$@

clean:
	rm -rf *.o

.PHONY: debug all clean recvserver addrinfo
