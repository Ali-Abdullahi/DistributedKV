#define _GNU_SOURCE
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "kv_store.h"
#include "followers.h"

#define MAX_FOLLOWERS 64
#define RECONNECT_DELAY_SEC 2


void replicate_data(const char *cmd, const char *key, const char *val) {
    if (node_mode != MODE_LEADER) return;

    int fds[MAX_FOLLOWERS];
    size_t n = followers_snapshot(fds, MAX_FOLLOWERS);
    if (n == 0) return;

    char buf[BUFSIZE];
    int len;
    if (val && *val) {
        len = snprintf(buf, sizeof(buf), "%s %s %s\n", cmd, key, val);
    } else {
        len = snprintf(buf, sizeof(buf), "%s %s\n", cmd, key);
    }
    if (len <= 0) return;

    int dead[MAX_FOLLOWERS];
    size_t dead_n = 0;

    for (size_t i = 0; i < n; i++) {
        ssize_t off = 0;
        int ok = 1;
        while (off < len) {
            ssize_t w = send(fds[i], buf + off, len - off, 0);
            if (w <= 0) { ok = 0; break; }
            off += w;
        }
        if (!ok) {
            fprintf(stderr, "Replication to fd=%d failed (%s), dropping\n",
                    fds[i], strerror(errno));
            dead[dead_n++] = fds[i];
        }
    }

    for (size_t i = 0; i < dead_n; i++) {
        followers_remove(dead[i]);
    }
}


static void apply_replicated(const char *line) {
    char cmd[16] = {0};
    char key[30] = {0};
    char val[100] = {0};
    int parsed = sscanf(line, "%15s %29s %99s", cmd, key, val);

    if (parsed == 3 && strcmp(cmd, "PUT") == 0) {
        kvPut(key, val);
        save_to_disk();
    } else if (parsed == 2 && strcmp(cmd, "DEL") == 0) {
        kvDel(key);
        save_to_disk();
    }
}


int follower_loop(const char *host, const char *port) {
    while (keep_going) {
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host, port, &hints, &res) != 0) {
            fprintf(stderr, "Follower: getaddrinfo failed, retry in %ds\n",
                    RECONNECT_DELAY_SEC);
            sleep(RECONNECT_DELAY_SEC);
            continue;
        }

        int fd = socket(res->ai_family, res->ai_socktype, 0);
        if (fd < 0) {
            perror("socket");
            freeaddrinfo(res);
            sleep(RECONNECT_DELAY_SEC);
            continue;
        }

        if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
            fprintf(stderr, "Follower: connect to %s:%s failed (%s), retry in %ds\n",
                    host, port, strerror(errno), RECONNECT_DELAY_SEC);
            close(fd);
            freeaddrinfo(res);
            sleep(RECONNECT_DELAY_SEC);
            continue;
        }
        freeaddrinfo(res);

        printf("Follower: connected to leader %s:%s, registering...\n", host, port);

        if (auth_token != NULL) {
            char auth[256];
            int alen = snprintf(auth, sizeof(auth), "AUTH %s\n", auth_token);
            if (alen <= 0 || send(fd, auth, alen, 0) < 0) {
                perror("send AUTH");
                close(fd);
                sleep(RECONNECT_DELAY_SEC);
                continue;
            }
        }

        const char *reg = "REGISTER\n";
        if (send(fd, reg, strlen(reg), 0) < 0) {
            perror("send REGISTER");
            close(fd);
            sleep(RECONNECT_DELAY_SEC);
            continue;
        }

        char buf[BUFSIZE];
        size_t used = 0;

        while (keep_going) {
            ssize_t r = read(fd, buf + used, BUFSIZE - 1 - used);
            if (r <= 0) break;
            used += r;
            buf[used] = '\0';

            char *start = buf;
            char *nl;
            while ((nl = strchr(start, '\n')) != NULL) {
                *nl = '\0';
                if (*start) apply_replicated(start);
                start = nl + 1;
            }
            size_t leftover = used - (start - buf);
            memmove(buf, start, leftover);
            used = leftover;
        }

        printf("Follower: lost connection to leader, reconnecting in %ds...\n",
               RECONNECT_DELAY_SEC);
        close(fd);
        if (keep_going) sleep(RECONNECT_DELAY_SEC);
    }
    return 0;
}
