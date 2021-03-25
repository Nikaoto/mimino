# Makefile
CC:= cc
CFLAGS := -pipe -O -Wall -Wextra -g
FAST_CFLAGS:= -pipe -O2 -Wall -Wpedantic -Wall -Wextra -g
LINK := $(CC)

all: mimino

mimino:
	$(CC) $(CFLAGS) mimino.c -o $@

fast:
	$(CC) $(FAST_CFLAGS) mimino.c -o $@

recvserver addrinfo:
	$(CC) $(CFLAGS) $@/*.c -o $@/$@

clean:
	rm -rf *.o

.PHONY: debug all clean recvserver addrinfo
