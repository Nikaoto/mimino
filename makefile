# Makefile
CC := cc
CWARNS := -Wall -Wpedantic -Wextra -Wno-comments -Wno-implicit-fallthrough -Wno-incompatible-pointer-types
CFLAGS := $(CWARNS) -g
FAST_CFLAGS := -O2 -Wall -Wpedantic -Wextra -g
LINK := $(CC)

all: mimino

# fast: mimino.c
# 	$(CC) $(FAST_CFLAGS) mimino.c -o $@

OBJS_DIR=.objs
OBJS= \
	$(OBJS_DIR)/buffer.o       \
	$(OBJS_DIR)/arg.o          \
	$(OBJS_DIR)/dir.o          \
	$(OBJS_DIR)/http.o         \
	$(OBJS_DIR)/xmalloc.o      \
	$(OBJS_DIR)/defer.o        \
	$(OBJS_DIR)/ascii.o        \
	$(OBJS_DIR)/connection.o   \

$(shell mkdir -p $(OBJS_DIR))

all: mimino

mimino: $(OBJS) $(OBJS_DIR)/mimino.o
	$(CC) $(CFLAGS) $(OBJS) $(OBJS_DIR)/mimino.o -o $@

$(OBJS_DIR)/%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

recvserver: recvserver/recvserver.c
	$(CC) $@/*.c $(CFLAGS) -o $@/$@

addrinfo: addrinfo/addrinfo.c
	$(CC) $@/*.c $(CFLAGS) -o $@/$@

clean:
	rm -rf $(OBJS_DIR)
	rm -rf tests/run_tests

test: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -I./ tests/test_main.c -o tests/run_tests
	@echo
	./tests/run_tests

install: mimino
	install -m755 ./mimino /usr/local/bin/.

uninstall:
	rm -rf /usr/local/bin/mimino

.PHONY: debug all clean install test recvserver addrinfo
