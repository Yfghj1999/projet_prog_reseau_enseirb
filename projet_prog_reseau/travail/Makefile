CC     = gcc
CFLAGS = -Iinclude -I. -Wall -Wextra

all: bin/client bin/server

bin/client: src/client.c src/protocol.c src/file_transfert.c include/*.h
	mkdir -p bin
	$(CC) $(CFLAGS) src/client.c src/protocol.c src/file_transfert.c -o $@

bin/server: src/server.c src/protocol.c include/*.h
	mkdir -p bin
	$(CC) $(CFLAGS) src/server.c src/protocol.c -o $@

clean:
	rm -rf bin inbox

.PHONY: all clean
