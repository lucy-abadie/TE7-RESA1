CFLAGS=-Wall
#LDFLAGS=-lpthread
all: client server

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

client: client.c common.o
	$(CC) $(CFLAGS) -o client client.c common.o

server: server.c common.o
	$(CC) $(CFLAGS) -o server server.c common.o

clean:
	rm -f client server common.o

