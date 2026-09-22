#ifndef COMMON_H
#define COMMON_H

#include <sys/socket.h>
#define MSG_LEN 1024
#define SERV_PORT "8080"
#define SERV_ADDR "127.0.0.1"
#define MAX_CLIENTS 128

struct info{
    short s;
    long l;
};

int send_all(int sockfd, const void *buf, size_t len);
int recv_all(int sockfd, void *buf, size_t len);

#endif