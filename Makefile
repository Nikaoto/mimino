# Makefile
CC:= cc
CFLAGS := -pipe -O -Wall -Wextra -g
FAST_CFLAGS:= -pipe -O2 -Wall -Wpedantic -Wall -Wextra -g
LINK := $(CC)

all: mimino

mimino: mimino.c
	$(CC) $(CFLAGS) mimino.c -o $@

fast: mimino.c
	$(CC) $(FAST_CFLAGS) mimino.c -o $@

recvserver: recvserver/recvserver.c
	$(CC) $(CFLAGS) $@/*.c -o $@/$@

addrinfo: addrinfo/addrinfo.c
	$(CC) $(CFLAGS) $@/*.c -o $@/$@

clean:
	rm -rf *.o

.PHONY: debug all clean recvserver addrinfo
