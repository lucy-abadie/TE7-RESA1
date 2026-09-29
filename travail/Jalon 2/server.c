#include "common.h"
#include "server.h"
#include "user_list.h"
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
#include <string.h>

#define MAX_MESSAGE_SIZE 4096
#define MAX_CLIENTS 128

int setup_listening_socket(int port) {
	int listen_fd;
	int result;
	struct sockaddr_in6 server_address; //Pour accepter IPv4 & IPv6

	listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
	die(listen_fd, "socket");
	printf("TCP listening socket created.\n");

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin6_family = AF_INET6;
	server_address.sin6_addr = in6addr_any; // To listen on all interfaces --- Equivalent to 0.0.0.0
	server_address.sin6_port = htons((unsigned short)port);
	result = bind(listen_fd, (struct sockaddr *)&server_address, sizeof(server_address));
	die(result, "bind");
	printf("Socket bound to port %d.\n", port);

	result = listen(listen_fd, 20);
	die(result, "listen");
	printf("Listening for client connections.\n");
	return listen_fd;
}

struct server {
    struct user_list *users;
};

void server_poll_loop(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS],
		struct server *srv) {
	int running = 1;

	/* Slot 0 is the listener. The other slots contain client sockets. */
	for (int i = 0; i < MAX_CLIENTS; i++) {
		poll_fds[i].fd = -1;
		poll_fds[i].events = 0;
		poll_fds[i].revents = 0;
	}
	poll_fds[0].fd = listen_fd;
	poll_fds[0].events = POLLIN;

    // execute server logic
	while (running) {
		int ready = poll(poll_fds, MAX_CLIENTS, -1);
		die(ready, "poll");

		//Accept client
		if ((poll_fds[0].revents & POLLIN) != 0) {
			struct sockaddr_storage client_addr; //accepte tout type d'adresse
            socklen_t len = sizeof(client_addr);
            int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &len);
            
            for (int slot = 1; slot < MAX_CLIENTS; slot++) {
                if (poll_fds[slot].fd < 0) {
                    user_list_add(srv->users, client_fd, (struct sockaddr *)&client_addr);
                    poll_fds[slot].fd = client_fd;
                    poll_fds[slot].events = POLLIN;
					poll_fds[slot].revents = 0;

                    //selon type adresse
                    char ip_str[INET6_ADDRSTRLEN];
					int client_port;

					if (client_addr.ss_family == AF_INET) { //IPv4
						struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
						inet_ntop(AF_INET, &s->sin_addr, ip_str, sizeof(ip_str));
						client_port = ntohs(s->sin_port);
					} else { //IPv6
						struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_addr;
						inet_ntop(AF_INET6, &s->sin6_addr, ip_str, sizeof(ip_str));
						client_port = ntohs(s->sin6_port);
					}

					printf("New connection accepted from %s:%d (slot %d)\n", ip_str, client_port, slot);
					break;

                }
            }
		}

		//Existing client
		for (int slot = 1; slot < MAX_CLIENTS; slot++) {
			short returned_events = poll_fds[slot].revents;
			int close_connection = 0;

			if (poll_fds[slot].fd < 0) {
				continue;
			}

			if ((returned_events & POLLIN) != 0) {
				struct message msg;
                void *payload = NULL;
                
                // Read msg
                if (protocol_recv_message(poll_fds[slot].fd, &msg, &payload) < 0) {
					close_connection = 1;
                } else {
                    struct user *sender = user_list_find_by_socket(srv->users, poll_fds[slot].fd);
                    server_route_message(srv, sender, &msg, payload);
                    if (payload) {
						free(payload);
					}
                }
            }
			if ((returned_events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
				close_connection = 1;
			}
			if (close_connection) {
				int client_fd = poll_fds[slot].fd;
				struct user *leaving_user = user_list_find_by_socket(srv->users, client_fd);
                if (leaving_user != NULL && strlen(leaving_user->nickname) > 0) {
                    printf("User '%s' (socket %d) disconnected.\n", leaving_user->nickname, client_fd);
                } else {
                    printf("Anonymous user (socket %d) disconnected.\n", client_fd);
                }
				close(client_fd);
				user_list_remove(srv->users, client_fd);
				poll_fds[slot].fd = -1;
				poll_fds[slot].events = 0;
				poll_fds[slot].revents = 0;
			}
		}
		if ((poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			running = 0;
		}
	}
	
	// Close client sockets and free client list
	for (int slot = 1; slot < MAX_CLIENTS; slot++) {
		if (poll_fds[slot].fd >= 0) {
			close(poll_fds[slot].fd);
			poll_fds[slot].fd = -1;
		}
	}

	user_list_destroy(srv->users);
}

int is_valid_nickname(const char *nick) {
    if (nick == NULL) {
        return 0; 
    }

    int longueur = 0;

    for (int i = 0; i < 128; i++) {
        if (nick[i] == '\0') {
            break; 
        }
        
        char c = nick[i];

        if (c == ' ') {
            return 0;
        }
        
        int est_une_majuscule = (c >= 'A' && c <= 'Z');
        int est_une_minuscule = (c >= 'a' && c <= 'z');
        int est_un_chiffre = (c >= '0' && c <= '9');
        
        if (est_une_majuscule == 0 && est_une_minuscule == 0 && est_un_chiffre == 0) {
            return 0; 
        }
        
        longueur = longueur + 1;
    }
    
    if (longueur == 0) {
        return 0; 
    }
    
    if (longueur >= 128) {
        return 0; 
    }

    return 1; 
}

int server_route_message(struct server *server, struct user *sender, const struct message *message, const void *payload) {
    struct message response;
    char response_payload[2048] = {0}; 

    memset(&response, 0, sizeof(response));
    strcpy(response.nick_sender, "Server");
    response.type = ECHO_SEND; 

    switch (message->type) {
        
        case NICKNAME_NEW: {
			//pseudo invalid
			if (is_valid_nickname(message->infos) == 0) {
                snprintf(response_payload, sizeof(response_payload), "Error : Invalid pseudo (letters and numbers only, without spaces, max %d characters).", NICK_LEN - 1);
            }
            // pseudo already existing
            else if (user_list_find_by_nickname(server->users, message->infos) != NULL) {
                snprintf(response_payload, sizeof(response_payload), "Error : pseudo '%s' already used.", message->infos);
            } else {
            // add pseudo
                strncpy(sender->nickname, message->infos, NICK_LEN - 1);
                snprintf(response_payload, sizeof(response_payload), "Welcome on the chat %s", sender->nickname);
            }
            response.pld_len = strlen(response_payload);
            protocol_send_message(sender->socket_fd, &response, response_payload);
            break;
        }

        case NICKNAME_LIST: {
            // /who : liste utilisateurs
            strcpy(response_payload, "Online users are\n");
            struct user *current = server->users->head;
            
            while (current != NULL) {
                strcat(response_payload, "           - ");
                if (strlen(current->nickname) > 0) {
                    strcat(response_payload, current->nickname);
                } else {
                    strcat(response_payload, "Unknown (without pseudo)");
                }
                strcat(response_payload, "\n");
                current = current->next;
            }
            response.pld_len = strlen(response_payload);
            protocol_send_message(sender->socket_fd, &response, response_payload);
            break;
        }

        case NICKNAME_INFOS: {
            // /whois <pseudo> : infos utilisateur ciblé
            struct user *target = user_list_find_by_nickname(server->users, message->infos);
            
            if (target != NULL) {
                snprintf(response_payload, sizeof(response_payload),
                         "%s connected since %s with IP address %s and port number %d",
                         target->nickname, target->connection_time, target->ip, target->port);
            } else {
                snprintf(response_payload, sizeof(response_payload), "Error : User '%s' does not exist.", message->infos);
            }
            response.pld_len = strlen(response_payload);
            protocol_send_message(sender->socket_fd, &response, response_payload);
            break;
        }

        case BROADCAST_SEND: {
            // /msgall : envoyer à tous sauf l'expéditeur
            struct user *current = server->users->head;
            while (current != NULL) {
                if (current->socket_fd != sender->socket_fd) { 
                    struct message bcast_msg = *message;
                    protocol_send_message(current->socket_fd, &bcast_msg, payload);
                }
                current = current->next;
            }
            break;
        }

        case UNICAST_SEND: {
            //  /msg <pseudo> : envoyer à un utilisateur précis
            struct user *target = user_list_find_by_nickname(server->users, message->infos);
            
            if (target != NULL) {
                struct message uni_msg = *message;
                protocol_send_message(target->socket_fd, &uni_msg, payload);
            } else {
                snprintf(response_payload, sizeof(response_payload), "Error : User '%s' can't be find.", message->infos);
                response.pld_len = strlen(response_payload);
                protocol_send_message(sender->socket_fd, &response, response_payload);
            }
            break;
        }

        case ECHO_SEND: {
            // on renvoie le message à l'expéditeur
            struct message echo_msg = *message;
            protocol_send_message(sender->socket_fd, &echo_msg, payload);
            break;
        }

        default:
            break;
    }
    
    return 0;
}

int main(int argc, char **argv) {
	struct pollfd poll_fds[MAX_CLIENTS];
	int port;
	int listen_fd;

	if (argc != 2) {
		fprintf(stderr, "Usage: ./server <server_port>\n");
		return EXIT_FAILURE;
	}
	port = atoi(argv[1]);
	if (port < 1 || port > 65535) {
		fprintf(stderr, "Invalid port\n");
		return EXIT_FAILURE;
	}

	struct server srv;
    struct user_list users;
    user_list_init(&users);
    srv.users = &users;

	listen_fd = setup_listening_socket(port);
	server_poll_loop(listen_fd, poll_fds, &srv);
	close(listen_fd);
	return EXIT_SUCCESS;
}
