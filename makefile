CFLAGS = -g

all:
	$(CC) $(CFLAGS) board.c -o engine
