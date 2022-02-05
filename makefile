# Makefile
CC := cc
CWARNS := -Wall -Wextra -Wno-comments -Wno-implicit-fallthrough
CFLAGS := -pipe -O $(CWARNS) -g $(DEFS)
FAST_CFLAGS := -pipe -O2 -Wall -Wpedantic -Wextra -g $(DEFS)
LINK := $(CC)

all: mimino

# fast: mimino.c
# 	$(CC) $(FAST_CFLAGS) mimino.c -o $@

mimino: mimino.h mimino.o ascii.h http.o dir.o
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
