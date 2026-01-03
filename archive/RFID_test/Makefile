all: main test 

main: main.c
	gcc main.c -o main -lgpiod

test: test.c 
	gcc test.c -o test -lgpiod

test2: test2.c
	gcc test2.c -o test2 -lgpiod

test3: test3.c
	gcc test3.c -o test3 -lgpiod 

clean:
	rm -rf test main test2 test3