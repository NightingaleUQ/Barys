CFLAGS = -Wall -Wpedantic -O3 -mmmx -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -msse4 -mavx -mavx2
.PHONY: debug clean

all: engine

debug: CFLAGS += -g -DDEBUG
debug: engine

engine: board.o
	$(CC) $(CFLAGS) $^ -o engine

board.o: board.c board.h

clean:
	rm engine
	rm *.o
	rm -rf history
