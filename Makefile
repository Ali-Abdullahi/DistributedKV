CFLAGS= -Wall -pthread -g -Wextra


kv_store.o : kv_store.c
	@echo "Compiling kvStore.c"
	gcc $(CFLAGS) -c kv_store.c

network.o :
