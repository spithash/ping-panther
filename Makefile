CC = gcc
CFLAGS = -I/usr/include/uv -Wall -Wextra
LDFLAGS = -luv

all: pingpanther

pingpanther: main.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: src/main.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o pingpanther

