#ifndef KV_STORE_H
#define KV_STORE_H
#include <pthread.h>

#define TABLE_SIZE 500
#define BUFSIZE 512

typedef struct Node{
    char *key;
    char *val;
    struct Node *next; //For collisions to hold more than one node at a specific array position
} Node;   

extern Node *kvStore[TABLE_SIZE];
extern pthread_rwlock_t rwlock;

unsigned int hash(char *key);
char * kvGet(const char * key);
void kvPut(const char *key, const char *val);
int kvDel(const char *key);

void save_to_disk();
void pull_from_disk();


#endif