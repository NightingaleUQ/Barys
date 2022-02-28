CFLAGS = -Wall -Wpedantic
.PHONY: debug clean

build: optimise

optimise: CFLAGS += -O3 -mmmx -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -msse4 -mavx -mavx2
optimise: engine

debug: CFLAGS += -g -DDEBUG
debug: engine

engine:
	$(CC) $(CFLAGS) board.c -o engine

clean:
	rm engine
	rm -rf history
