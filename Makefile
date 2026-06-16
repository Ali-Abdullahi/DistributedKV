CFLAGS= -Wall -pthread -g -Wextra

all: kv
	rm -f *.o

kv : main.o persistence.o network.o kv_store.o replication.o followers.o client.o
	gcc $(CFLAGS) main.o persistence.o network.o kv_store.o replication.o followers.o client.o -o kv

replication.o : replication.c
	gcc $(CFLAGS) -c replication.c

followers.o : followers.c
	gcc $(CFLAGS) -c followers.c

kv_store.o : kv_store.c
	gcc $(CFLAGS) -c kv_store.c

network.o : network.c
	gcc $(CFLAGS) -c network.c

persistence.o : persistence.c
	gcc $(CFLAGS) -c persistence.c

client.o : client.c
	gcc $(CFLAGS) -c client.c

main.o : main.c
	gcc $(CFLAGS) -c main.c

clean:
	rm -f kv final *.o
