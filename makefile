# Author: Gurbinder Singh
# File: Makefile for hashsum
CC = gcc
DEFS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c99 -pedantic -Wall $(DEFS)

OBJ = hashsum.o

.PHONY: all clean

all: hashsum

hashsum: $(OBJ)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -g -c $^
	

clean:
	rm -f *.o *.gch
