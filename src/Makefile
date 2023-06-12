CC = gcc
CFLAGS = -Wall -Werror -g
LDFLAGS = -lreadline

all: main

main: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o main main.c

run: main
	./main

debug: main
	gdb main

clean:
	rm -f main
