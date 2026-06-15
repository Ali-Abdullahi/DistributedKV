#define _GNU_SOURCE

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "kv_store.h"


Node *kvStore[TABLE_SIZE]= {NULL};
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
node_mode_t node_mode = MODE_LEADER;


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


}

int kvDel(const char *key){
    unsigned int idx = hash((char*)key);
    Node *curr= kvStore[idx];
    Node *prev= NULL;
    int deleted=0;

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

    return deleted;

}


void sync_state_to_fd(int fd) {
    char **keys = NULL;
    char **vals = NULL;
    size_t n = 0, cap = 0;

    pthread_rwlock_rdlock(&rwlock);
    for (int i = 0; i < TABLE_SIZE; i++) {
        Node *curr = kvStore[i];
        while (curr != NULL) {
            if (n == cap) {
                cap = cap ? cap * 2 : 16;
                keys = realloc(keys, cap * sizeof(char *));
                vals = realloc(vals, cap * sizeof(char *));
            }
            keys[n] = strdup(curr->key);
            vals[n] = strdup(curr->val);
            n++;
            curr = curr->next;
        }
    }
    pthread_rwlock_unlock(&rwlock);

    char buf[BUFSIZE];
    for (size_t i = 0; i < n; i++) {
        int len = snprintf(buf, sizeof(buf), "PUT %s %s\n", keys[i], vals[i]);
        if (len > 0) {
            ssize_t off = 0;
            while (off < len) {
                ssize_t w = send(fd, buf + off, len - off, 0);
                if (w <= 0) break;
                off += w;
            }
        }
        free(keys[i]);
        free(vals[i]);
    }
    free(keys);
    free(vals);
}


