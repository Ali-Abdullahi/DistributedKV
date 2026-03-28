#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "kv_store.h"

char *follower_port = NULL;
char *follower_ip= NULL;

void replicate_data(const char *cmd, const char *key, const char *val){
    if(follower_port==NULL || follower_ip){
        return;
    }

    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(follower_ip, follower_port, &hints, &res) != 0){
        return;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == -1) {
        freeaddrinfo(res);
        return;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
            close(sock);
            freeaddrinfo(res);
            return; 
    }

    char buffer[BUFSIZE];
    snprintf(buffer, sizeof(buffer), "%s %s %s\n", cmd, key, val);

    ssize_t sent = send(sock, buffer, strlen(buffer), 0);

    if (sent < (ssize_t)strlen(buffer)) {
        if (sent < 0) {
            perror("Send error");
        } else {
            fprintf(stderr, "Partial send occurred: only sent %zd bytes\n", sent);
        }
    }
    close(sock);
    freeaddrinfo(res);
}