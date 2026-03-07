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


   
Node *kvStore[TABLE_SIZE]= {NULL};
// int writers_waiting = 0; // Don't get reader locks if writers are waiting.
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


unsigned int hash(char *key){
    unsigned int hash_val=0;
    for(int i=0; key[i]!='\0' ;i++){
        hash_val+= ((hash_val<<5)+ hash_val) + key[i];
    }
    return hash_val % TABLE_SIZE;
}


char* kvGet(const char *key) {
    unsigned int idx = hash((char*)key);
    char *result = NULL;

    pthread_rwlock_rdlock(&rwlock);
    Node *curr = kvStore[idx];
    while(curr != NULL) {
        if(strcmp(curr->key, key) == 0) {
            result = strdup(curr->val);
            break;
        }
        curr = curr->next;
    }
    pthread_rwlock_unlock(&rwlock);
    
    return result; 
}

void kvPut(const char *key, const char*val){
    unsigned int idx = hash((char*)key);
    int checked=0;
    pthread_rwlock_wrlock(&rwlock);

    Node *curr= kvStore[idx];
    while(curr != NULL){
        if(strcmp(curr->key,key)==0){
            free(curr->val);
            curr->val=strdup(val);
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

     // Todo, I need to remember to keep put my persistance storage here

}

int kvDel(const char *key){
    unsigned int idx = hash((char*)key);
    char *result = NULL;
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

}


