CFLAGS = -O3 -Wall -Wpedantic

.PHONY: debug clean

all:
	$(CC) $(CFLAGS) board.c -o engine

debug: CFLAGS += -g
debug: all

