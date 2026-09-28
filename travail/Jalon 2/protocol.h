#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

struct message;

int protocol_send_all(int socket_fd, const void *buffer, size_t length);
int protocol_recv_all(int socket_fd, void *buffer, size_t length);
int protocol_send_message(int socket_fd, const struct message *message,
                          const void *payload);
int protocol_recv_message(int socket_fd, struct message *message,
                          void **payload);
int protocol_validate_message(const struct message *message);

#endif
