CFLAGS = -Wall -Wpedantic -mmmx -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -msse4 -mavx -mavx2
.PHONY: optimise debug clean

all: optimise

boris: boris.o board.o
	$(CC) $(CFLAGS) -o boris $^ -lm -lpthread

boris.o: boris.c
board.o: board.c board.h

optimise: CFLAGS += -O3
optimise: boris

debug: CFLAGS += -g -DDEBUG
debug: boris

clean:
	rm boris
	rm *.o
	rm -rf history
