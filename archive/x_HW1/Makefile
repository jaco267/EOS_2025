CC = gcc
all: main

main:   hw1_prototype.c
	$(CC) -o $@ hw1_prototype.c -lm

.PHONY: run
run:  
	./main

.PHONY: clean
clean:
	rm -f *.o main