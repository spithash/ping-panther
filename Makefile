CC = gcc
CFLAGS = -I/usr/include/ncurses -Wall -Wextra
LDFLAGS = -lncurses

all: pingpanther

pingpanther: main.o
	$(CC) -o $@ main.o $(LDFLAGS)

main.o: src/main.c
	$(CC) $(CFLAGS) -c src/main.c -o main.o

clean:
	rm -f *.o pingpanther

