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

    char cmd_input[BUFSIZE];
    ssize_t bytes_read= read(client_fd, cmd_input, BUFSIZE-1);

    if(bytes_read>0){
        cmd_input[bytes_read]='\0';
        char cmd[5];
        char key[30];
        char val[100];

        int parse_cmd= sscanf(cmd_input, "%s %s %s", cmd, key, val);

        char *res= "COMPLETE\n";

        if (strcmp(cmd, "PUT")== 0 && parse_cmd== 3){
            kvPut(key,val);
            save_to_disk();
            write(client_fd, "COMPLETE\n", 9);
        }

        else if(strcmp(cmd, "GET") == 0 && parse_cmd == 2){
            char* val_found= kvGet(key);
            if(val_found != NULL){
                write(client_fd,val_found,strlen(val_found));
                write(client_fd,"\n",1);
            }

        }
        else if(strcmp(cmd, "DEL") == 0 && parse_cmd == 2){
            unsigned int idx= hash(key);
            Node *curr= kvStore[idx];
            Node *prev= NULL;
            int deleted=0;

            printf("User DEL: Key=%s\n", key);

            pthread_rwlock_wrlock(&rwlock);
            while(curr!=NULL){
                if(strcmp(curr->key,key)==0){
                    if(prev==NULL){
                        kvStore[idx]= curr->next;
                    }
                    else{
                        prev->next= curr->next;
                    }
                    free(curr->key);
                    free(curr->val);
                    free(curr);
                    deleted=1;
                    break;
                }
                prev= curr;
                curr= curr->next;
            }
            pthread_rwlock_unlock(&rwlock);

            if(deleted==1){
                save_to_disk(); // Saves to disk for persistence
                log_op("DEL", key, "OK", NULL); //Display timestamp for DEL
                write(client_fd, res, strlen(res));
            }
            else{
                log_op("DEL", key, "NOT_FOUND", NULL); //Display timestamp for DEL
                char *err_msg= "KEY NOT FOUND!\n";
                write(client_fd, err_msg, strlen(err_msg));
            }
        }

        else{
            char *err_msg= "INVALID COMMAND!\n";
            write(client_fd, err_msg, strlen(err_msg));
        }

    }

    close(client_fd);
    return NULL;
}


void network_server(int argc, char **argv) {
    struct sigaction scheck;
    scheck.sa_handler= handle_sigint;
    scheck.sa_flags=0;
    sigemptyset(&scheck.sa_mask);
    if(sigaction(SIGINT,&scheck,NULL)==-1){
        perror("sigaction");
        return -1;

    }
  
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    const char *port = argv[1];
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

    pull_from_disk(); // Loads data from disk if exists
    pthread_rwlock_init(&rwlock, NULL);

    printf("KV-Store Server started on port %s\n", port);
    printf("Ready for connections...\n");
    while(keep_going==1){   // Main Logic for PUT, GET, Everything above is the base for building a TCP server
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
        // Switch implementation to thread pool at some point
        pthread_detach(tempThread);
    }

    pthread_rwlock_destroy(&rwlock);
    close(sock_fd);
    return 0;
}