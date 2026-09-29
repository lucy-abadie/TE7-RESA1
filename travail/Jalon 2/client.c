#include "common.h"
#include "client.h"
#include "msg_struct.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#define MAX_MESSAGE_SIZE 4096

int setup_connection(const char *server_ip, const char *server_port) {
	int socket_fd;
	struct addrinfo hints, *res, *p;

	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; //IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(server_ip, server_port, &hints, &res) != 0) {
        fprintf(stderr, "Invalid address or port: %s:%s\n", server_ip, server_port);
        return -1;
    }

	for (p = res; p != NULL; p = p->ai_next) {
        socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        if (connect(socket_fd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // Connexion réussie
        }

        close(socket_fd);
        socket_fd = -1;
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to connect to %s:%s.\n", server_ip, server_port);
        freeaddrinfo(res);
        return -1;
    }

    printf("Connected to %s on port %s.\n", server_ip, server_port);
    freeaddrinfo(res);
    return socket_fd;
}

char current_nick[NICK_LEN] = "";

int client_handle_input(int socket_fd) {
    char input[1024];
    if (fgets(input, sizeof(input), stdin) == NULL) return 0;
    input[strcspn(input, "\n")] = '\0';

    struct message msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
    
    char payload[1024] = {0};

    if (strncmp(input, "/quit", 5) == 0) {
        return 0; // Terminer la connexion
    } else if (strncmp(input, "/nick ", 6) == 0) {
        msg.type = NICKNAME_NEW;
        strncpy(msg.infos, input + 6, INFOS_LEN - 1);
		strncpy(current_nick, input + 6, NICK_LEN - 1);
    } else if (strcmp(input, "/who") == 0) {
        msg.type = NICKNAME_LIST;
    } else if (strncmp(input, "/whois ", 7) == 0) {
        msg.type = NICKNAME_INFOS;
        strncpy(msg.infos, input + 7, INFOS_LEN - 1);
    } else if (strncmp(input, "/msgall ", 8) == 0) {
        msg.type = BROADCAST_SEND;
        strncpy(payload, input + 8, sizeof(payload) - 1);
        msg.pld_len = strlen(payload);
    } else if (strncmp(input, "/msg ", 5) == 0) {
        msg.type = UNICAST_SEND;
        char *target = input + 5;
        char *space = strchr(target, ' ');
        if (space) {
            *space = '\0';
            strncpy(msg.infos, target, INFOS_LEN - 1);
            strncpy(payload, space + 1, sizeof(payload) - 1);
            msg.pld_len = strlen(payload);
        }
    } else {
        msg.type = ECHO_SEND;
        strncpy(payload, input, sizeof(payload) - 1);
        msg.pld_len = strlen(payload);
    }

    protocol_send_message(socket_fd, &msg, msg.pld_len > 0 ? payload : NULL);
    return 1;
}

void client_poll_loop(int socket_fd) {
	struct pollfd watched[2];
	int running = 1;

	/* Initialize once; poll() fills revents after each call. */
	watched[0].fd = STDIN_FILENO;
	watched[0].events = POLLIN;
	watched[1].fd = socket_fd;
	watched[1].events = POLLIN;

	printf("Connected. Enter /nick <pseudo> to identify.\n");

	while (running) {
		int ready = poll(watched, 2, -1);
		die(ready, "poll");

		//Server
		if ((watched[1].revents & POLLIN) != 0) {
			struct message msg;
            void *payload = NULL;
            
            if (protocol_recv_message(socket_fd, &msg, &payload) < 0) {
                printf("Disconnected.\n");
                running = 0;
            } else {
                if (payload) {
                    if (strlen(msg.nick_sender) > 0) {
                        printf("[%s] : %s\n", msg.nick_sender, (char*)payload);
                    } else {
                        printf("%s\n", (char*)payload);
                    }
                    free(payload);
                }
            }
		}

		//Keyboard
		if (running && (watched[0].revents & POLLIN) != 0) {
			running = client_handle_input(socket_fd);
		}

		if ((watched[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 || (watched[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			running = 0;
		}
	}
}

int main(int argc, char **argv) {
	int socket_fd;

	if (argc != 3) {
		fprintf(stderr, "Usage: ./client <server_ip> <server_port>\n");
		return EXIT_FAILURE;
	}
	socket_fd = setup_connection(argv[1], argv[2]);
	if (socket_fd < 0) {
		return EXIT_FAILURE;
	}
	client_poll_loop(socket_fd);
	close(socket_fd);
	return EXIT_SUCCESS;
}