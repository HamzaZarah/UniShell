CC = gcc
CFLAGS = -Wall -Werror -g

all: main

main: main.c
	$(CC) $(CFLAGS) -o main main.c

run: main
	./main

debug: main
	gdb main

clean:
	rm -f main
