#include "user_list.h"
#include "msg_struct.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

void user_list_init(struct user_list *users) {
    users->head = NULL;
}

int user_list_add(struct user_list *users, int socket_fd, struct sockaddr_in *addr) {
    if (users == NULL || addr == NULL) {
        return -1;
    }

    struct user *new_user = calloc(1, sizeof(struct user));
    if (!new_user) {
        return -1;
    }

    // Remplissage des champs
    new_user->socket_fd = socket_fd;
    inet_ntop(AF_INET, &(addr->sin_addr), new_user->ip, INET_ADDRSTRLEN);
    new_user->port = ntohs(addr->sin_port);
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(new_user->connection_time, sizeof(new_user->connection_time), "%Y/%m/%d at %H:%M", t);

    // Insertion en tête de liste
    new_user->next = users->head;
    users->head = new_user;
    
    return 0;
}

int user_list_remove(struct user_list *users, int socket_fd) {
    struct user **cursor = &(users->head);

    while (*cursor != NULL) {
        if ((*cursor)->socket_fd == socket_fd) {
            struct user *removed = *cursor;
            *cursor = removed->next;
            free(removed);
            return 0;
        }
        cursor = &(*cursor)->next;
    }
    return -1; 
}

void user_list_destroy(struct user_list *users) {
    struct user *current = users->head;

    while (current != NULL) {
        struct user *removed = current;
        current = current->next;
        free(removed);
    }
    users->head = NULL;
}

struct user *user_list_find_by_nickname(struct user_list *users, const char *nickname) {
    struct user *current = users->head;
    while (current != NULL) {
        if (strcmp(current->nickname, nickname) == 0) return current;
        current = current->next;
    }
    return NULL;
}

struct user *user_list_find_by_socket(struct user_list *users, int socket_fd) {
    struct user *current = users->head;
    while (current != NULL) {
        if (current->socket_fd == socket_fd) return current;
        current = current->next;
    }
    return NULL;
}