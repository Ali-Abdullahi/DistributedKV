#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "kv_store.h"


#define LISTEN_QUEUE_LEN 5



void *handle_command(void *client_fd_ptr) {
    int client_fd = *((int *) client_fd_ptr);
    free(client_fd_ptr);

    char *welcome = "CONNECTED to Distributed-KV v1.0\n> ";
    write(client_fd, welcome, strlen(welcome));

    char cmd_input[BUFSIZE];
    memset(cmd_input, 0, BUFSIZE);
    while(1){
        ssize_t bytes_read= read(client_fd, cmd_input, BUFSIZE-1);

        if(bytes_read>0){
            cmd_input[bytes_read]='\0';
            printf("Follower received: %s\n", cmd_input);
            char cmd[5];
            char key[30];
            char val[100];

            int parse_cmd= sscanf(cmd_input, "%s %s %s", cmd, key, val);

            char *res= "COMPLETE\n";

            if (strcmp(cmd, "PUT")== 0 && parse_cmd== 3){
                kvPut(key,val);
                save_to_disk();
                replicate_data("PUT", key, val);
                write(client_fd, "COMPLETE\n", 9);
            }

            else if(strcmp(cmd, "GET") == 0 && parse_cmd == 2){
                char* val_found= kvGet(key);
                if(val_found != NULL){
                    write(client_fd,val_found,strlen(val_found));
                    write(client_fd,"\n",1);
                    free(val_found);
                }
                else{
                    write(client_fd, "NOT_FOUND\n", 10);
                }

            }
            else if(strcmp(cmd, "DEL") == 0 && parse_cmd == 2){
                if(kvDel(key)==1){
                save_to_disk();
                replicate_data("DEL", key, "");
                write(client_fd, "COMPLETE\n", 9);              
                }
                else{
                    write(client_fd,"KEY NOT FOUND\n",16);
                }

            }

            else{
                char *err_msg= "INVALID COMMAND!\n";
                write(client_fd, err_msg, strlen(err_msg));
            }
        }
    }
    close(client_fd);
    return NULL;
}



// NETWORK SERVER
int network_server(const char *port) {
    struct addrinfo hints;
    memset(&hints,0,sizeof(hints));
    hints.ai_family= AF_UNSPEC;
    hints.ai_socktype= SOCK_STREAM;
    hints.ai_flags= AI_PASSIVE;
    struct addrinfo *servers;

    if ((getaddrinfo(NULL,port, &hints,&servers)) != 0) {
        fprintf(stderr, "getaddrinfo error");
        return -1;
    }
    int sock_fd;
    if ((sock_fd = socket(servers->ai_family, servers->ai_socktype, 0)) == -1) {
        perror("socket");
        freeaddrinfo(servers);
        return -1;
    }

    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(sock_fd,servers->ai_addr,servers->ai_addrlen)==-1){
        perror("bind");
        freeaddrinfo(servers);
        close(sock_fd);
        return -1;
    }
    freeaddrinfo(servers);
    if(listen(sock_fd,LISTEN_QUEUE_LEN)==-1){
        perror("listen");
        close(sock_fd);
        return -1;

    }


    printf("KV-Store Server started on port %s\n", port);
    printf("Ready for connections...\n");

    while(keep_going==1){
        int client_fd= accept(sock_fd,NULL,NULL);
        if(client_fd==-1){
            if(errno==EINTR){
                break;
            }
            perror("accept");
            pthread_rwlock_destroy(&rwlock);
            close(sock_fd);
            return -1;
        }

        pthread_t tempThread;
        // Safer than passing directly
        int *client_fd_ptr = (int *) malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        pthread_create(&tempThread, NULL, handle_command, client_fd_ptr);
        pthread_detach(tempThread);
    }

    pthread_rwlock_destroy(&rwlock);
    close(sock_fd);
    return 0;
}