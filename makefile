CFLAGS = -Wall -Wpedantic -O3 -mmmx -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -msse4 -mavx -mavx2
.PHONY: debug clean

boris: boris.o board.o
	$(CC) $(CFLAGS) -o boris $^ -lm

boris.o: boris.c
board.o: board.c board.h

debug: CFLAGS += -g -DDEBUG
debug: boris

clean:
	rm boris
	rm *.o
	rm -rf history
