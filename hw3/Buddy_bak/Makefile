#Sample Makefile for Malloc
CC=gcc
CFLAGS=-g -O0 -fPIC

all:	check

clean:
	rm -rf libmalloc.so malloc.o

libmalloc.so: malloc.o
	$(CC) $(CFLAGS) -shared -Wl,--unresolved-symbols=ignore-all $< -o $@

test1: test1.o
	$(CC) $(CFLAGS) $< -o $@

# For every XYZ.c file, generate XYZ.o.
%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

check:	libmalloc.so test1
	LD_PRELOAD=`pwd`/libmalloc.so ./test1

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar

mymalloc:
	gcc mymalloc.c -g -o m.o -lm
