CC = gcc
CFLAGS = -Wall -Wextra

all: vm

vm: vm.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f vm