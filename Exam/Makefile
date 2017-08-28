CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -g

all:
	$(CC) $(CFLAGS) client.c -o client
	$(CC) $(CFLAGS) server.c -o server

client: client.c
	$(CC) $(CFLAGS) $^ -o $@

server: server.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f client
	rm -f server

# How to run:
# Start server with: ./server <job-file> <port>
# Start client with: ./client <IP/Name> <port>

