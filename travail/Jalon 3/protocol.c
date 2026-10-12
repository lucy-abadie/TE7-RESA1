#include "protocol.h"
#include "msg_struct.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int protocol_send_all(int socket_fd, const void *buffer, size_t length) {
   size_t total = 0;
    const char *cursor = buffer;

    while (total < length) {
        ssize_t n = write(socket_fd, cursor + total, length - total);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

int protocol_recv_all(int socket_fd, void *buffer, size_t length) {
    size_t total = 0;
    char *cursor = buffer;

    while (total < length) {
        ssize_t n = read(socket_fd, cursor + total, length - total);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

int protocol_send_message(int socket_fd, const struct message *message, const void *payload) {
    if (protocol_send_all(socket_fd, message, sizeof(struct message)) < 0) {
        return -1;
    } 

    if (message->pld_len > 0 && payload != NULL) {
        if (protocol_send_all(socket_fd, payload, message->pld_len) < 0){
            return -1;
        }
    }
    return 0;
}

int protocol_recv_message(int socket_fd, struct message *message, void **payload) {
    if (protocol_recv_all(socket_fd, message, sizeof(struct message)) < 0) {
        return -1;
    }
    
    if (message->pld_len > 0) {
        *payload = calloc(1, message->pld_len + 1);
        if (*payload == NULL) {
            return -1;
        }
        
        if (protocol_recv_all(socket_fd, *payload, message->pld_len) < 0) {
            free(*payload);
            *payload = NULL;
            return -1;
        }
    } else {
        *payload = NULL;
    }
    return 0;
}