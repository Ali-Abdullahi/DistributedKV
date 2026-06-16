#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "kv_store.h"

#define CLIENT_BUFSIZE 1024

static ssize_t read_one_line(int fd, char *acc, size_t *used, size_t cap, char *out, size_t outsize) {
    while (1) {
        for (size_t i = 0; i < *used; i++) {
            if (acc[i] == '\n') {
                size_t copy = i < outsize - 1 ? i : outsize - 1;
                memcpy(out, acc, copy);
                out[copy] = 0;
                memmove(acc, acc + i + 1, *used - i - 1);
                *used -= i + 1;
                return (ssize_t)copy;
            }
        }
        if (*used >= cap - 1) return -1;
        ssize_t n = read(fd, acc + *used, cap - 1 - *used);
        if (n <= 0) return -1;
        *used += n;
    }
}

static int connect_to(const char *host, const char *port) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        fprintf(stderr, "Cannot resolve %s:%s\n", host, port);
        return -1;
    }
    int fd = socket(res->ai_family, res->ai_socktype, 0);
    if (fd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return -1;
    }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        fprintf(stderr, "Cannot connect to %s:%s (%s)\n", host, port, strerror(errno));
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return fd;
}

static void to_upper(char *s) {
    for (; *s; s++) *s = toupper((unsigned char)*s);
}

int run_client(const char *host, const char *port) {
    int fd = connect_to(host, port);
    if (fd < 0) return 1;

    char acc[CLIENT_BUFSIZE];
    size_t used = 0;
    char line[CLIENT_BUFSIZE];

    if (read_one_line(fd, acc, &used, sizeof(acc), line, sizeof(line)) < 0) {
        fprintf(stderr, "Server closed connection unexpectedly\n");
        close(fd);
        return 1;
    }
    printf("Connected to leader at %s:%s\n", host, port);
    char *fc = strstr(line, "followers connected: ");
    if (fc) printf("  (%s\n", fc);

    if (auth_token != NULL) {
        char auth[256];
        int alen = snprintf(auth, sizeof(auth), "AUTH %s\n", auth_token);
        if (alen <= 0 || send(fd, auth, alen, 0) < 0) {
            perror("send AUTH");
            close(fd);
            return 1;
        }
        if (read_one_line(fd, acc, &used, sizeof(acc), line, sizeof(line)) < 0) {
            fprintf(stderr, "Auth: no response\n");
            close(fd);
            return 1;
        }
        if (strcmp(line, "OK") != 0) {
            fprintf(stderr, "Auth failed: %s\n", line);
            close(fd);
            return 1;
        }
        printf("  (authenticated)\n");
    }

    printf("\nType: put <key> <value>  |  get <key>  |  del <key>  |  quit\n\n");

    char input[CLIENT_BUFSIZE];
    while (1) {
        printf("kv> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) { printf("\n"); break; }
        input[strcspn(input, "\r\n")] = 0;
        if (input[0] == 0) continue;
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0 || strcmp(input, "q") == 0) break;

        char verb[16] = {0}, key[64] = {0}, val[256] = {0};
        int parsed = sscanf(input, "%15s %63s %255s", verb, key, val);
        if (parsed < 1) continue;
        to_upper(verb);

        char wire[CLIENT_BUFSIZE];
        int wlen;
        int expect_two_lines = 1;

        if (strcmp(verb, "PUT") == 0) {
            if (parsed != 3) { printf("usage: put <key> <value>\n"); continue; }
            wlen = snprintf(wire, sizeof(wire), "PUT %s %s\n", key, val);
        } else if (strcmp(verb, "GET") == 0) {
            if (parsed != 2) { printf("usage: get <key>\n"); continue; }
            wlen = snprintf(wire, sizeof(wire), "GET %s\n", key);
        } else if (strcmp(verb, "DEL") == 0) {
            if (parsed != 2) { printf("usage: del <key>\n"); continue; }
            wlen = snprintf(wire, sizeof(wire), "DEL %s\n", key);
        } else {
            printf("unknown command '%s' — try put/get/del/quit\n", verb);
            continue;
        }

        if (send(fd, wire, wlen, 0) < 0) {
            fprintf(stderr, "Send failed: %s\n", strerror(errno));
            break;
        }

        char primary[CLIENT_BUFSIZE], secondary[CLIENT_BUFSIZE] = {0};
        if (read_one_line(fd, acc, &used, sizeof(acc), primary, sizeof(primary)) < 0) {
            fprintf(stderr, "Server closed connection\n");
            break;
        }
        if (expect_two_lines) {
            if (read_one_line(fd, acc, &used, sizeof(acc), secondary, sizeof(secondary)) < 0) {
                secondary[0] = 0;
            }
        }

        int followers = -1;
        if (strncmp(secondary, "FOLLOWERS ", 10) == 0) {
            followers = atoi(secondary + 10);
        }

        if (strcmp(verb, "PUT") == 0) {
            if (strcmp(primary, "COMPLETE") == 0) {
                if (followers > 0)      printf("OK (replicated to %d follower%s)\n", followers, followers==1?"":"s");
                else if (followers == 0) printf("OK (no followers connected)\n");
                else                    printf("OK\n");
            } else {
                printf("%s\n", primary);
            }
        } else if (strcmp(verb, "GET") == 0) {
            if (strcmp(primary, "NOT_FOUND") == 0) {
                printf("(not found)\n");
            } else {
                printf("%s\n", primary);
            }
        } else if (strcmp(verb, "DEL") == 0) {
            if (strcmp(primary, "COMPLETE") == 0) {
                if (followers > 0)      printf("OK (replicated to %d follower%s)\n", followers, followers==1?"":"s");
                else if (followers == 0) printf("OK (no followers connected)\n");
                else                    printf("OK\n");
            } else if (strcmp(primary, "KEY NOT FOUND") == 0) {
                printf("(key not found)\n");
            } else {
                printf("%s\n", primary);
            }
        }
    }

    close(fd);
    printf("Goodbye.\n");
    return 0;
}
