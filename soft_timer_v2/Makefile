CC = gcc
CFLAGS = -Wall -O2
SRC = soft_timer.c call_back.c soft_timer_api.c glob_var.c

soft_timer: $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f soft_timer 