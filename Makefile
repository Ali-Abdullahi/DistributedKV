CFLAGS= -Wall -pthread -g -Wextra

all: final

final : main.o persistence.o network.o kv_store.o
	gcc $(CFLAGS) main.o persistence.o network.o kv_store.o -o final

kv_store.o : kv_store.c
	gcc $(CFLAGS) -c kv_store.c

network.o : network.c 
	gcc $(CFLAGS) -c network.c 

persistence.o : persistence.c 
	gcc $(CFLAGS) -c persistence.c 

main.o : main.c 
	gcc $(CFLAGS) -c main.c

