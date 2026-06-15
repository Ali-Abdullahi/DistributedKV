#ifndef KV_STORE_H
#define KV_STORE_H
#include <pthread.h>

#define TABLE_SIZE 500
#define BUFSIZE 512

typedef struct Node {
    char *key;
    char *val;
    struct Node *next;
} Node;

typedef enum {
    MODE_LEADER,
    MODE_FOLLOWER
} node_mode_t;

extern Node *kvStore[TABLE_SIZE];
extern pthread_rwlock_t rwlock;
extern node_mode_t node_mode;
extern int keep_going;

unsigned int hash(char *key);
char *kvGet(const char *key);
void kvPut(const char *key, const char *val);
int kvDel(const char *key);

void save_to_disk(void);
void pull_from_disk(void);

void replicate_data(const char *cmd, const char *key, const char *val);
void sync_state_to_fd(int fd);
int follower_loop(const char *leader_host, const char *leader_port);

int network_server(const char *port);
void *handle_command(void *client_fd_ptr);

#endif
