#include <signal.h>
#include <stdio.h>

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

}