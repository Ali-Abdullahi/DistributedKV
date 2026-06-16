#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "kv_store.h"
#include "followers.h"

int keep_going = 1;

void handle_sigint(int signo) {
    (void)signo;
    printf("\nShutting down gracefully...\n");
    keep_going = 0;
}

static void print_local_ips(void) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }
    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        if (sa->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) continue;
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sa->sin_addr, addr, sizeof(addr));
        printf("  %-6s %s\n", ifa->ifa_name, addr);
        found = 1;
    }
    if (!found) printf("  (no non-loopback IPv4 addresses found — are you on a network?)\n");
    freeifaddrs(ifaddr);
}

static void print_leader_banner(const char *port) {
    printf("\nLeader running on port %s.\n", port);
    printf("Share one of these IPs with followers / clients:\n");
    print_local_ips();
    printf("\nOn the other machine they'd run:\n");
    printf("  kv follower <your-ip> %s\n", port);
    printf("  kv client   <your-ip> %s\n\n", port);
}

static int run_leader(const char *port) {
    node_mode = MODE_LEADER;
    pull_from_disk();
    print_leader_banner(port);
    return network_server(port);
}

static int run_follower(const char *host, const char *port) {
    node_mode = MODE_FOLLOWER;
    printf("Role: FOLLOWER (leader at %s:%s)\n", host, port);
    return follower_loop(host, port);
}

static void strip_newline(char *s) {
    s[strcspn(s, "\r\n")] = 0;
}

static int prompt(const char *label, char *out, size_t outsize) {
    printf("%s", label);
    fflush(stdout);
    if (!fgets(out, outsize, stdin)) return -1;
    strip_newline(out);
    return (out[0] == 0) ? -1 : 0;
}

static void interactive_menu(void) {
    printf("\n=== Distributed KV Store ===\n\n");
    printf("Your IP address(es) on this machine:\n");
    print_local_ips();
    printf("\nWhat would you like to do?\n");
    printf("  1) Start as Leader      (pick a port; share your IP with followers)\n");
    printf("  2) Start as Follower    (connect to a leader by IP and port)\n");
    printf("  3) Connect as Client    (interactive REPL; needs leader IP and port)\n");
    printf("  q) Quit\n\n");

    char choice[16];
    if (prompt("> ", choice, sizeof(choice)) != 0) return;

    if (strcmp(choice, "1") == 0) {
        char port[16];
        if (prompt("Port to listen on: ", port, sizeof(port)) != 0) return;
        run_leader(port);
    } else if (strcmp(choice, "2") == 0) {
        char host[64], port[16];
        if (prompt("Leader IP: ",   host, sizeof(host)) != 0) return;
        if (prompt("Leader port: ", port, sizeof(port)) != 0) return;
        run_follower(host, port);
    } else if (strcmp(choice, "3") == 0) {
        char host[64], port[16];
        if (prompt("Leader IP: ",   host, sizeof(host)) != 0) return;
        if (prompt("Leader port: ", port, sizeof(port)) != 0) return;
        run_client(host, port);
    }
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s                                   # interactive menu\n", prog);
    fprintf(stderr, "  %s leader   <port>                   # start as leader\n", prog);
    fprintf(stderr, "  %s follower <leader_ip> <port>       # start as follower\n", prog);
    fprintf(stderr, "  %s client   <leader_ip> <port>       # interactive REPL client\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Optional: set KV_TOKEN=<secret> on every node to require token auth.\n");
}

int main(int argc, char **argv) {
    struct sigaction scheck;
    scheck.sa_handler = handle_sigint;
    scheck.sa_flags = 0;
    sigemptyset(&scheck.sa_mask);
    if (sigaction(SIGINT, &scheck, NULL) == -1) {
        perror("sigaction");
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    auth_token = getenv("KV_TOKEN");
    if (auth_token && *auth_token == 0) auth_token = NULL;

    pthread_rwlock_init(&rwlock, NULL);
    followers_init();

    int rc = 0;
    if (argc == 1) {
        interactive_menu();
    } else if (argc == 3 && strcmp(argv[1], "leader") == 0) {
        rc = run_leader(argv[2]);
    } else if (argc == 4 && strcmp(argv[1], "follower") == 0) {
        rc = run_follower(argv[2], argv[3]);
    } else if (argc == 4 && strcmp(argv[1], "client") == 0) {
        rc = run_client(argv[2], argv[3]);
    } else {
        usage(argv[0]);
        rc = 1;
    }

    if (node_mode == MODE_LEADER || node_mode == MODE_FOLLOWER) {
        printf("Finalizing persistence before exit...\n");
        save_to_disk();
    }
    followers_destroy();
    pthread_rwlock_destroy(&rwlock);
    return rc;
}
