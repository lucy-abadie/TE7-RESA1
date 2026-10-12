#ifndef USER_LIST_H
#define USER_LIST_H

#include "msg_struct.h"
#include <netinet/in.h>
#include <arpa/inet.h>

struct user {
    int socket_fd;
    char nickname[NICK_LEN];
    char ip[INET6_ADDRSTRLEN];
    int port;
    char connection_time[64];
    struct user *next;
};

struct user_list {
    struct user *head;
};

void user_list_init(struct user_list *users);
struct user *user_list_find_by_nickname(struct user_list *users, const char *nickname);
struct user *user_list_find_by_socket(struct user_list *users, int socket_fd);
int user_list_add(struct user_list *users, int socket_fd, struct sockaddr *addr);
int user_list_remove(struct user_list *users, int socket_fd);
void user_list_destroy(struct user_list *users);

#endif
