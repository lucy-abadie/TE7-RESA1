#ifndef SERVER_H
#define SERVER_H

struct server;
struct user;
struct message;

int server_listen(struct server *server, const char *port);
int server_run(struct server *server);
int server_accept_client(struct server *server);
int server_receive_message(struct server *server, struct user *sender);
int server_handle_message(struct server *server, struct user *sender,
                          const struct message *message, const void *payload);
int server_route_message(struct server *server, struct user *sender,
                         const struct message *message, const void *payload);
int server_forward_file_request(struct server *server, struct user *sender,
                                const struct message *message, const void *payload);
void server_close(struct server *server);

#endif
