#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "common.h"

#define FDS_SIZE 128
#define IPv4_SIZE 16

struct client_info{
	int fd;
    char ip[IPv4_SIZE];
    int port;
    struct client_info *next;
};

struct client_info *clients_head = NULL;

void add_client(int fd, struct sockaddr_in *addr) {
    struct client_info *new_client = malloc(sizeof(struct client_info));
    new_client->fd = fd;
	new_client->port = ntohs(addr->sin_port);
    inet_ntop(AF_INET, &(addr->sin_addr), new_client->ip, IPv4_SIZE);
    new_client->next = clients_head;
    clients_head = new_client;
    fprintf(stdout, "Connexion acepted : IP (%s) - Port (%d) (fd %d)\n", new_client->ip, new_client->port, fd);
}

void remove_client(int fd) {
    struct client_info *curr = clients_head;
    struct client_info *prev = NULL;
    while (curr != NULL) {
        if (curr->fd == fd) {
            if (prev == NULL) {
                clients_head = curr->next;
            } else {
                prev->next = curr->next;
            }
            fprintf(stdout, "Client disconected: IP (%s) - Port (%d) (fd %d)\n", curr->ip, curr->port, fd);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

int echo_server(int sockfd) {
	char buff[MSG_LEN];
    int msg_size = 0;
    
    // Size of msg
    if (recv_all(sockfd, &msg_size, sizeof(int)) <= 0){
		return -1;
	}
    
    // Msg
    memset(buff, 0, MSG_LEN);
    if (recv_all(sockfd, buff, msg_size) <= 0){
		return -1;
	}
    
    fprintf(stdout, "Received (from fd %i): %s\n", sockfd, buff);
    
    //quit
    if (strncmp(buff, "/quit", 5) == 0){
		return -1;
	}

    //send back to client (size + msg)
    if (send_all(sockfd, &msg_size, sizeof(int)) == -1){
		return -1;
	}
    if (send_all(sockfd, buff, msg_size) == -1){
		return -1;
	}
    
    fprintf(stdout, "Message sent (to fd %i)!\n", sockfd);
    return 0;
}

int handle_bind(const char *port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(NULL, port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
		int yes = 1;
    	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
		if (sfd == -1) {
			continue;
		}
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {
	if(argc != 2){
		fprintf(stdout,"./server <server_port>");
		exit(EXIT_FAILURE);
	}

	int sfd = handle_bind(argv[1]);

	if ((listen(sfd, SOMAXCONN)) != 0) {
			perror("listen()\n");
			exit(EXIT_FAILURE);
		}

	struct pollfd fds[MAX_CLIENTS];
    fds[0].fd = sfd;
    fds[0].events = POLLIN;
	fds[0].revents = 0;
    for (int i = 1; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
    }

    fprintf(stdout, "Server starting on port %s...\n", argv[1]);

    while (1) {
        if (poll(fds, MAX_CLIENTS, -1) == -1) {
            perror("poll()");
            break;
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (fds[i].fd == -1 || fds[i].revents == 0){
				continue;
			}

            if (fds[i].fd == sfd) {
                // New connexion
                struct sockaddr_in cli;
                socklen_t len = sizeof(cli);
                int connfd = accept(sfd, (struct sockaddr*) &cli, &len);
                if (connfd < 0) {
                    perror("accept()\n");
                    continue;
                }
                
                add_client(connfd, &cli);
                
                for (int j = 1; j < MAX_CLIENTS; j++) {
                    if (fds[j].fd == -1) {
                        fds[j].fd = connfd;
                        fds[j].events = POLLIN;
						fds[j].revents = 0;
                        break;
                    }
                }
            } else {
                // Close socket
                if (echo_server(fds[i].fd) == -1) {
                    close(fds[i].fd);
                    remove_client(fds[i].fd);
                    fds[i].fd = -1;
                }
            }
        }
    }

	close(sfd);
	return EXIT_SUCCESS;
}

