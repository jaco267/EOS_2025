CC = gcc
CFLAGS = -Wall -O2
SRC_client = room_client.c 

SRC_test   = test_client.c
SRC_server = room_server_/room_server.c   room_server_/room_action.c  \
  room_server_/globe_var.c

all: room_client room_server test_client

room_client: $(SRC_client)
	$(CC) $(CFLAGS) -o $@ $^


room_server: $(SRC_server)
	$(CC) $(CFLAGS) -o $@ $^

test_client: $(SRC_test)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f room_client room_server test_client