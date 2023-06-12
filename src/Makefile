CC = gcc
CFLAGS = -Wall -Werror -g

all: main

main: main.c
	$(CC) $(CFLAGS) main.c -o main -lpthread -lreadline

run: main
	./main

debug: main
	gdb main

clean:
	rm -f main

