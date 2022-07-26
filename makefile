# Makefile
CC := cc
CWARNS := -w -Wno-comments -Wno-implicit-fallthrough
CFLAGS := -pipe -O $(CWARNS) -g
FAST_CFLAGS := -pipe -O2 -Wall -Wpedantic -Wextra -g
LINK := $(CC)

all: mimino

# fast: mimino.c
# 	$(CC) $(FAST_CFLAGS) mimino.c -o $@

mimino: mimino.h ascii.h defer.h xmalloc.h mimino.o xmalloc.o http.o dir.o
	$(CC) $^ $(CFLAGS) -o $@

%.o: %.c %.h
	$(CC) $< $(CFLAGS) -c -o $@

recvserver: recvserver/recvserver.c
	$(CC) $@/*.c $(CFLAGS) -o $@/$@

addrinfo: addrinfo/addrinfo.c
	$(CC) $@/*.c $(CFLAGS) -o $@/$@

clean:
	rm -rf *.o

.PHONY: debug all clean recvserver addrinfo
