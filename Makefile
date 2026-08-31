CFLAGS=-Wall -Wextra -Wpedantic -Werror -std=c99 -ggdb

all:
	$(CC) $(CFLAGS) -o db *.c