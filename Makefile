HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.c)

sshpass: $(HEADERS) $(SOURCES)
	c99 -o $@ $^ -D_POSIX_SOURCE -lssh2 -lpthread
