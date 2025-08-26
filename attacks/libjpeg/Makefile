CC=gcc
CFLAGS=-O0 -std=c99 -Wall

all: idct_islow_port

idct_islow_port: idct_islow_port.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f idct_islow_port

