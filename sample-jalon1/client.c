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

void echo_client(int sockfd) {
	struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
	fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;
	fds[1].revents = 0;

	char buff[MSG_LEN];
	fprintf(stdout, "Connected. Enter msg (/quit to quit) :\n");

	while (1) {
		int nb_active_fd = poll(fds, 2, -1);
		if (nb_active_fd == -1) {
            perror("poll()");
            break;
        }

        // Keyboard
        if (fds[0].revents & POLLIN) {
			
            memset(buff, 0, MSG_LEN);

			if (fgets(buff, sizeof(buff), stdin) == NULL){
				break;
			}

            buff[strcspn(buff, "\n")] = '\0';
            int msg_size = strlen(buff);

			int net_size = htonl(msg_size);
            
            // msg size + msg
            if (send_all(sockfd, &net_size, sizeof(int)) == -1) {
				break;
			}
            if (send_all(sockfd, buff, msg_size) == -1) {
				break;
			}

			//quit
            if (strcmp(buff, "/quit") == 0) {
                fprintf(stdout, "Closed connexion...\n");
                break;
            }
        }

        // Received data from server
        if (fds[1].revents & POLLIN) {
            int msg_size = 0;
            //size
            if (recv_all(sockfd, &msg_size, sizeof(int)) <= 0) {
                fprintf(stdout, "\nDisconected server\n");
                break;
            }

			msg_size = ntohl(msg_size);
            
            // msg
			char *msg = malloc(msg_size + 1);

			if (msg == NULL) {
				perror("malloc a échoué");
				break;
			}

            if (recv_all(sockfd, msg, msg_size) <= 0){
				free(msg);
				break;
			}
            
            printf("Received: %s\n", msg);
			free(msg);
        }
	}
}

int handle_connect(const char* server_name, const char* server_port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(server_name, server_port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char *argv[]) {
	if(argc != 3){
		fprintf(stdout,"./client <server_name> <server_port>");
		exit(EXIT_FAILURE);
	}

	int sfd;

	sfd = handle_connect(argv[1], argv[2]);
	echo_client(sfd);
	close(sfd);

	return EXIT_SUCCESS;
}

