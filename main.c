#include <signal.h>
#include <stdio.h>
#include "kv_store.h"

int keep_going=1;

void handle_sigint(int signo) {
    printf("\nShutting down gracefully...\n");
    keep_going = 0;
}

int main(int argc, char **argv) {
    struct sigaction scheck;
    scheck.sa_handler= handle_sigint;
    scheck.sa_flags=0;
    sigemptyset(&scheck.sa_mask);
    if(sigaction(SIGINT,&scheck,NULL)==-1){
        perror("sigaction");
        return -1;

    }
  
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    pthread_rwlock_init(&rwlock, NULL);
    pull_from_disk();

    network_server(argv[1]);

    printf("Finalizing persistence before exit...\n");
    save_to_disk();
    
    pthread_rwlock_destroy(&rwlock);

    return 0;

}