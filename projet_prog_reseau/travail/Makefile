CC     = gcc
CFLAGS = -Iinclude -Wall -Wextra

all: bin/client bin/server

bin/client: client.c protocol.c include/*.h
	mkdir -p bin
	$(CC) $(CFLAGS) client.c protocol.c -o $@

bin/server: server.c protocol.c include/*.h
	mkdir -p bin
	$(CC) $(CFLAGS) server.c protocol.c -o $@

clean:
	rm -rf bin

.PHONY: all clean
