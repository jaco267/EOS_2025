CC = gcc
CFLAGS = -Wall -O2
SRC_client = room_client.c 

SRC_server = room_server.c 

all: room_client room_server

room_client: $(SRC_client)
	$(CC) $(CFLAGS) -o $@ $^


room_server: $(SRC_server)
	$(CC) $(CFLAGS) -o $@ $^


clean:
	rm -f room_client room_server