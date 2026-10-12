#define _GNU_SOURCE

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
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/stat.h>

#define MAX_MESSAGE_SIZE 4096

char current_nick[NICK_LEN] = "";
char pending_request_sender[NICK_LEN] = "";
char pending_request_filename[256] = "";
char pending_send_filepath[512] = "";
long expected_file_size = 0;
long total_received_bytes = 0;

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

int setup_file_transfer_listening_socket(int *port_out) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return -1;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0; 
    
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) return -1;
    if (listen(listen_fd, 1) < 0) return -1;
    
    socklen_t len = sizeof(addr);
    if (getsockname(listen_fd, (struct sockaddr *)&addr, &len) == 0) {
        *port_out = ntohs(addr.sin_port);
    }
    return listen_fd;
}



int client_handle_input(int socket_fd, int *transfer_listen_fd) {
    char input[1024];
    if (fgets(input, sizeof(input), stdin) == NULL) return 0;
    input[strcspn(input, "\n")] = '\0';

    // réponse Y/N 
    if (pending_request_sender[0] != '\0') {
        if (strcasecmp(input, "Y") == 0) {
            int port = 0;
            *transfer_listen_fd = setup_file_transfer_listening_socket(&port);
            
            struct message msg; memset(&msg, 0, sizeof(msg));
            msg.type = FILE_ACCEPT;
            strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
            strncpy(msg.infos, pending_request_sender, INFOS_LEN - 1);
            
            char payload[128];
            snprintf(payload, sizeof(payload), "127.0.0.1:%d", port); // Simplification TP: Localhost
            msg.pld_len = strlen(payload);
            protocol_send_message(socket_fd, &msg, payload);
            
        } else { // Refus (N ou autre)
            struct message msg; memset(&msg, 0, sizeof(msg));
            msg.type = FILE_REJECT;
            strncpy(msg.nick_sender, current_nick, NICK_LEN - 1);
            strncpy(msg.infos, pending_request_sender, INFOS_LEN - 1);
            protocol_send_message(socket_fd, &msg, NULL);
            pending_request_sender[0] = '\0';
        }
        return 1;
    }

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
    } 
    else if (strncmp(input, "/send ", 6) == 0) {
        char *target = input + 6;
        char *space = strchr(target, ' ');
        if (space) {
            *space = '\0';
            char *filepath = space + 1;
            
            // Nettoyage des guillemets éventuels
            if (filepath[0] == '"') { 
				filepath++; filepath[strlen(filepath)-1] = '\0'; 
			}

            //Fichier existe ?
            FILE *check_file = fopen(filepath, "rb");
            if (check_file == NULL) {
                printf("Error : File '%s' can't be found.\n", filepath);
                return 1; 
            }
            fclose(check_file);

            //taille fichier
            struct stat st;
            if (stat(filepath, &st) < 0) {
                printf("Error : File '%s' can't be found.\n", filepath);
                return 1; 
            }
            off_t file_size = st.st_size;

            strncpy(pending_send_filepath, filepath, sizeof(pending_send_filepath)-1);
            
            // Extraction du nom de fichier pur
            char *filename = strrchr(filepath, '/');
            filename = filename ? filename + 1 : filepath;

            //envoi nom + taille
            char filename_and_size[512];
            snprintf(filename_and_size, sizeof(filename_and_size), "%s:%ld", filename, (long)file_size);

            msg.type = FILE_REQUEST;
            strncpy(msg.infos, target, INFOS_LEN - 1);
            msg.pld_len = strlen(filename_and_size);
            protocol_send_message(socket_fd, &msg, filename_and_size);
        }
        return 1;
    } 
    else {
        msg.type = ECHO_SEND;
        strncpy(payload, input, sizeof(payload) - 1);
        msg.pld_len = strlen(payload);
    }

    protocol_send_message(socket_fd, &msg, msg.pld_len > 0 ? payload : NULL);
    return 1;
}

void client_poll_loop(int socket_fd) {
	struct pollfd watched[4];
	// 0:clavier / 1:serveur / 2:écoute client-client / 3:écoute client-client entrante
	int running = 1;
	int transfer_listen_fd = -1;
    int incoming_transfer_fd = -1;
    FILE *received_file = NULL;

    /* Initialize once; poll() fills revents after each call. */
	for (int i = 0; i < 4; i++) { 
		watched[i].fd = -1; 
		watched[i].events = POLLIN; 
	}
	watched[0].fd = STDIN_FILENO;
	watched[1].fd = socket_fd;

	printf("Connected. Enter /nick <pseudo> to identify.\n");

	while (running) {
		int ready = poll(watched, 4, -1);
		die(ready, "poll");

		// Server
		if ((watched[1].revents & POLLIN) != 0) {
			struct message msg;
            void *payload = NULL;
            
            if (protocol_recv_message(socket_fd, &msg, &payload) < 0) {
                printf("Disconnected.\n");
                running = 0;
            } else {
				if (msg.type == FILE_REQUEST) {
                    strncpy(pending_request_sender, msg.nick_sender, NICK_LEN - 1);
                    sscanf((char*)payload, "%[^:]:%ld", pending_request_filename, &expected_file_size); //recup nom + taille
                    printf("%s wants you to accept the transfer of \"%s\" (%ld bytes). Do you accept? [Y/N]\n", msg.nick_sender, pending_request_filename, expected_file_size);
                } 
                else if (msg.type == FILE_REJECT) {
                    printf("%s rejected the file transfer.\n", msg.nick_sender);
                } 
                else if (msg.type == FILE_ACCEPT) {
                    printf("%s accepted file transfer.\nConnecting to %s and sending the file...\n", msg.nick_sender, msg.nick_sender);
                    char ip[64]; 
                    int port;
                    sscanf((char*)payload, "%[^:]:%d", ip, &port);
                    char port_str[16]; 
                    snprintf(port_str, sizeof(port_str), "%d", port);
                    
                    int outgoing_transfer_fd = setup_connection(ip, port_str);
                    if (outgoing_transfer_fd >= 0) {
                        FILE *f = fopen(pending_send_filepath, "rb");
                        if (f) {
                            char buffer[MAX_MESSAGE_SIZE]; 
                            size_t bytes;

                            // envoi morceaux message 
                            while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) { //morceaux de taille MAX_MESSAGE_SIZE
                                struct message s_msg; 
                                memset(&s_msg, 0, sizeof(s_msg));
                                s_msg.type = FILE_SEND; 
                                s_msg.pld_len = bytes;                                  
                                protocol_send_message(outgoing_transfer_fd, &s_msg, buffer);
                            }
                            fclose(f);
                            
                            //envoi morceau vide pour la fin
                            struct message eof_msg; 
                            memset(&eof_msg, 0, sizeof(eof_msg));
                            eof_msg.type = FILE_SEND; 
                            eof_msg.pld_len = 0;
                            protocol_send_message(outgoing_transfer_fd, &eof_msg, NULL);
                            
                            //attente ack
                            struct message ack_msg; 
                            void *ack_pld = NULL;
                            if (protocol_recv_message(outgoing_transfer_fd, &ack_msg, &ack_pld) == 0 && ack_msg.type == FILE_ACK) {
                                printf("%s has received the file.\n", msg.nick_sender);
                            }
                            if (ack_pld) free(ack_pld);
                        } else {
                            printf("Error: file can't be opened %s\n", pending_send_filepath);
                        }
                        close(outgoing_transfer_fd);
                    }
                } else {
                    if (payload) {
                        if (strlen(msg.nick_sender) > 0) {
                            printf("[%s] : %s\n", msg.nick_sender, (char*)payload);
                        } else {
                            printf("%s\n", (char*)payload);
                        }
                    }
                }
            }
            if (payload) {
                free(payload);
            }
		}

		// Keyboard
		if (running && (watched[0].revents & POLLIN) != 0) {
			running = client_handle_input(socket_fd, &transfer_listen_fd);
		}

		if (transfer_listen_fd != -1 && watched[2].fd == -1) {
			watched[2].fd = transfer_listen_fd;
		}

		// Connexion client-client entrante
        if (transfer_listen_fd != -1 && (watched[2].revents & POLLIN) != 0) {
            incoming_transfer_fd = accept(transfer_listen_fd, NULL, NULL);
            close(transfer_listen_fd); transfer_listen_fd = -1; watched[2].fd = -1;
            watched[3].fd = incoming_transfer_fd;
            
            mkdir(".resa1", 0777); mkdir(".resa1/inbox", 0777);
            char out_path[512]; 
            snprintf(out_path, sizeof(out_path), ".resa1/inbox/%s", pending_request_filename);
            received_file = fopen(out_path, "wb");
            printf("Receiving the file from %s...\n", pending_request_sender);
        }

        // Réception des morceaux de fichier
        if (incoming_transfer_fd != -1 && (watched[3].revents & POLLIN) != 0) {
            struct message file_chunk_msg; 
            void *file_chunk_payload = NULL;
            if (protocol_recv_message(incoming_transfer_fd, &file_chunk_msg, &file_chunk_payload) < 0) {
                close(incoming_transfer_fd); 
                incoming_transfer_fd = -1; 
                watched[3].fd = -1;
                if (received_file) { 
					fclose(received_file); 
                    received_file = NULL; 
				}
                total_received_bytes = 0;
            } else {
                if (file_chunk_msg.type == FILE_SEND) { //ecriture
                    if (file_chunk_msg.pld_len > 0 && file_chunk_payload != NULL) {
                        fwrite(file_chunk_payload, 1, file_chunk_msg.pld_len, received_file); 
                        total_received_bytes += file_chunk_msg.pld_len;
                    } else { //eof recu
                        fclose(received_file); 
                        received_file = NULL;
                        if (total_received_bytes == expected_file_size) {
                            fprintf(stdout, "Success: %s saved in .resa1/inbox/%s (%ld / %ld bytes)\n", pending_request_filename, pending_request_filename, total_received_bytes, expected_file_size);

                        //envoi ack
                        struct message ack_msg; memset(&ack_msg, 0, sizeof(ack_msg));
                        ack_msg.type = FILE_ACK;
                        protocol_send_message(incoming_transfer_fd, &ack_msg, NULL);
                        }
                        else{
                            fprintf(stdout, "Error: Received %ld bytes instead of %ld.\n", total_received_bytes, expected_file_size);
                        }

                        total_received_bytes = 0;
                        expected_file_size = 0;
                        close(incoming_transfer_fd); 
                        incoming_transfer_fd = -1; 
                        watched[3].fd = -1;
                        pending_request_sender[0] = '\0';
                    }
                }
                if (file_chunk_payload) {
					free(file_chunk_payload);
				} 
            }
        }

		// Gestion erreurs
		for (int i = 0; i < 4; i++) {
            if (watched[i].fd != -1 && (watched[i].revents & (POLLERR|POLLHUP|POLLNVAL))) {
                if (i == 0 || i == 1) running = 0;
            }
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