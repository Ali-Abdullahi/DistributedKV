#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kv_store.h"
#include "followers.h"

int keep_going = 1;

void handle_sigint(int signo) {
    (void)signo;
    printf("\nShutting down gracefully...\n");
    keep_going = 0;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <port>                                # leader (standalone until followers join)\n", prog);
    fprintf(stderr, "  %s --follower <leader_ip> <leader_port>  # follower; connects out to a leader\n", prog);
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
    // Don't die from SIGPIPE if a follower vanishes mid-send; we want EPIPE
    // from send() so replicate_data can prune the dead follower.
    signal(SIGPIPE, SIG_IGN);

    pthread_rwlock_init(&rwlock, NULL);
    followers_init();

    if (argc == 2) {
        node_mode = MODE_LEADER;
        pull_from_disk();
        printf("Role: LEADER on port %s (followers may join dynamically)\n", argv[1]);
        network_server(argv[1]);
    }
    else if (argc == 4 && strcmp(argv[1], "--follower") == 0) {
        node_mode = MODE_FOLLOWER;
        // Don't pull_from_disk in follower mode: leader's full state sync
        // is the source of truth on (re)connect.
        printf("Role: FOLLOWER (leader at %s:%s)\n", argv[2], argv[3]);
        follower_loop(argv[2], argv[3]);
    }
    else {
        usage(argv[0]);
        return 1;
    }

    printf("Finalizing persistence before exit...\n");
    save_to_disk();

    followers_destroy();
    pthread_rwlock_destroy(&rwlock);
    return 0;
}
