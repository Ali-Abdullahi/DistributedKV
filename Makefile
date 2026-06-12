CFLAGS= -Wall -pthread -g -Wextra

all: final
	rm -f *.o

final : main.o persistence.o network.o kv_store.o replication.o followers.o
	gcc $(CFLAGS) main.o persistence.o network.o kv_store.o replication.o followers.o -o final

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

main.o : main.c 
	gcc $(CFLAGS) -c main.c

clean:
	rm -f final *.o

