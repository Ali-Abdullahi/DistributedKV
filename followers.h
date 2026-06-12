#ifndef FOLLOWERS_H
#define FOLLOWERS_H
#include <stddef.h>

void followers_init(void);
void followers_add(int fd);
size_t followers_snapshot(int *out, size_t max);
void followers_remove(int fd);
size_t followers_count(void);
void followers_destroy(void);

#endif
