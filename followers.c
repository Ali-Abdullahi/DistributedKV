#define _GNU_SOURCE
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "followers.h"

#define MAX_FOLLOWERS 64

static int fds[MAX_FOLLOWERS];
static size_t n = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void followers_init(void) {
    pthread_mutex_lock(&lock);
    n = 0;
    pthread_mutex_unlock(&lock);
}

void followers_add(int fd) {
    pthread_mutex_lock(&lock);
    if (n < MAX_FOLLOWERS) {
        fds[n++] = fd;
    }
    pthread_mutex_unlock(&lock);
}

size_t followers_snapshot(int *out, size_t max) {
    pthread_mutex_lock(&lock);
    size_t k = n < max ? n : max;
    memcpy(out, fds, k * sizeof(int));
    pthread_mutex_unlock(&lock);
    return k;
}

void followers_remove(int fd) {
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < n; i++) {
        if (fds[i] == fd) {
            fds[i] = fds[n - 1];
            n--;
            close(fd);
            break;
        }
    }
    pthread_mutex_unlock(&lock);
}

size_t followers_count(void) {
    pthread_mutex_lock(&lock);
    size_t k = n;
    pthread_mutex_unlock(&lock);
    return k;
}

void followers_destroy(void) {
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < n; i++) close(fds[i]);
    n = 0;
    pthread_mutex_unlock(&lock);
}
