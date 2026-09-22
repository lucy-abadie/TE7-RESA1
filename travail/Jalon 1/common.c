#include "common.h"

int send_all(int sockfd, const void *buf, size_t len) {
    size_t total = 0;
    int bytesleft = len;
    int n;
    while(total < len) {
        n = send(sockfd, (char*)buf + total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    return n == -1 ? -1 : 0; 
}

// Fonction pour garantir la réception de l'intégralité des octets
int recv_all(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    int bytesleft = len;
    int n;
    while(total < len) {
        n = recv(sockfd, (char*)buf + total, bytesleft, 0);
        if (n <= 0) { return n; }
        total += n;
        bytesleft -= n;
    }
    return total; 
}

