CFLAGS=-Wall
#LDFLAGS=-lpthread
all: client server

client: client.o protocol.o
server: server.o protocol.o

clean:
	rm -f client server *.o