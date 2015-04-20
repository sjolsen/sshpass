HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.c)

sshpass: $(HEADERS) $(SOURCES)
	c99 -D_POSIX_SOURCE -o $@ $^
