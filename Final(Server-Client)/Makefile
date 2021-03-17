CC = gcc
CFLAGS =  -Wall -pthread

all: clean server client
	
server: clean
	$(CC) server.c Graph.c queue.c stack.c data_pool.c print_log.c cache.c -o server $(CFLAGS)

client: clean
	$(CC) client.c -o client $(CFLAGS)

run_server: all
	./server -i practice.txt -p 5000 -o log.txt -s 5 -x 10

clean:
	rm -f *.o server *.o client