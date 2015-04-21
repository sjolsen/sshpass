HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.c)

CFLAGS=-g
LDFLAGS=-lssh2 -lpthread -lgcrypt

sshpass: $(HEADERS) $(SOURCES)
	c99 -o $@ $^ -D_POSIX_SOURCE $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f sshpass
