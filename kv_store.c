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
#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define TABLE_SIZE 500

typedef struct Node{
    char *key;
    char *val;
    struct Node *next; //For collisions to hold more than one node at a specific array position
} Node;   

Node *kvStore[TABLE_SIZE]= {NULL};

unsigned int hash(char *key){
    unsigned int hash_val=0;
    for(int i=0; key[i]!='\0' ;i++){
        hash_val+= ((hash_val<<5)+ hash_val) + key[i];
    }
    return hash_val % TABLE_SIZE;
}

int keep_going = 1;

pthread_rwlock_t rwlock;
// int writers_waiting = 0; // Don't get reader locks if writers are waiting?
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static const char *LOG_FILE = "server.log";

void handle_sigint(int signo) {
    keep_going = 0;
}
//Timestamp/Logging function
static void log_op(const char *op, const char *key, const char *status, const char *value_opt) {
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now);

    pthread_mutex_lock(&log_mutex);

    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        if (value_opt) {
            fprintf(f, "[%s] %s key=%s value=%s status=%s\n", ts, op, key, value_opt, status);
        } else {
            fprintf(f, "[%s] %s key=%s status=%s\n", ts, op, key, status);
        }
        fclose(f);
    }

    pthread_mutex_unlock(&log_mutex);
}

void save_to_disk(){     // Saves the current contents of the kv_store into a database.txt file
    FILE *f= fopen("database.txt", "w");

    if(f==NULL){
        perror("Failed to open Database file.");
    }

    for(int i=0; i<TABLE_SIZE; i++){
        Node *curr= kvStore[i];
        while(curr!=NULL){
            fprintf(f, "%s %s\n", curr->key, curr->val);
            curr=curr->next;
        }
    }
    fclose(f);
}

void pull_from_disk(){  //Reloads the kv_store with the saved values from database.txt
    FILE *f= fopen("database.txt", "r");
    if(f==NULL){
        return;
    }

    char key[30];
    char val[100];

    while (fscanf(f, "%s %s",key, val) == 2){
        unsigned int idx= hash(key);
        Node *single_node= malloc(sizeof(Node));
        single_node->key= strdup(key);
        single_node->val= strdup(val);
        single_node->next= kvStore[idx];
        kvStore[idx]=single_node;
    }
    fclose(f);
    printf("Loaded data from disk.\n");    
}

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
            unsigned int idx= hash(key);
            Node* curr= kvStore[idx];
            int checked=0;

            pthread_rwlock_wrlock(&rwlock);
            //Check for same key name if it already exists we override
            while(curr!=NULL){
                if (strcmp(curr->key,key)==0){
                    free(curr->val);
                    curr->val= strdup(val);
                    checked=1;
                    break;
                }
                curr= curr->next;
            }

            if(checked!=1){
                Node * new_kv= malloc(sizeof(Node));
                new_kv->key=strdup(key); //Creates permanent copies of the data
                new_kv->val=strdup(val); //Creates permanent copies of the data
                new_kv->next= kvStore[idx]; //Points next to whatever is already at idx, so every new collision is at the beginning of the linked list.
                kvStore[idx]= new_kv;
            }
            pthread_rwlock_unlock(&rwlock);
            save_to_disk(); // Saves to disk for persistence
            log_op("PUT", key, "OK", val); //Display timestamp for PUT
            printf("User PUT: Key=%s, Val=%s\n", key, val);
            write(client_fd, res, strlen(res));
        }

        else if(strcmp(cmd, "GET") == 0 && parse_cmd == 2){
            unsigned int idx= hash(key);
            Node *curr= kvStore[idx];
            int obtained=0;

            printf("User GET: Key=%s\n", key);
            
            pthread_rwlock_rdlock(&rwlock);
            while(curr!=NULL){
                if(strcmp(curr->key,key)==0){
                    log_op("GET", key, "OK", curr->val); //Display timestamp for GET
                    write(client_fd, curr->val, strlen(curr->val));
                    write(client_fd, "\n", 1);
                    obtained=1;
                    break;
                }
                curr=curr->next;
            }
            pthread_rwlock_unlock(&rwlock);

            if(obtained!=1){
                log_op("GET", key, "NOT_FOUND", NULL); //Display timestamp for GET
                char *err_msg= "KEY NOT FOUND!\n";
                write(client_fd, err_msg, strlen(err_msg));
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

int main(int argc, char **argv) {
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