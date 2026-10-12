CC = gcc
CFLAGS = -Wall -Wextra -g

all: client server

client: client.c common.c protocol.c common.h protocol.h msg_struct.h client.h
	$(CC) $(CFLAGS) client.c common.c protocol.c -o client

server: server.c common.c protocol.c user_list.c common.h protocol.h msg_struct.h user_list.h server.h
	$(CC) $(CFLAGS) server.c common.c protocol.c user_list.c -o server

clean:
	rm -f client server

.PHONY: all clean