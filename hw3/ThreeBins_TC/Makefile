#Sample Makefile for Malloc
CC=gcc
CFLAGS=-g -O0 -fPIC -lpthread

all:	check

clean:
	rm -rf libmalloc.so malloc.o

libmalloc.so: malloc.o
	$(CC) $(CFLAGS) -shared -Wl,--unresolved-symbols=ignore-all $< -o $@

test1: test1.o
	# $(CC) $(CFLAGS) $< -o $@
	$(CC) $< $(CFLAGS) -o $@

t-test1: t-test1.o
	# $(CC) $(CFLAGS) $< -o $@
	$(CC) $< $(CFLAGS) -o $@

# For every XYZ.c file, generate XYZ.o.
%.o: %.c
	# $(CC) $(CFLAGS) $< -c -o $@
	$(CC) $< $(CFLAGS) -c -o $@

check:	libmalloc.so test1
	LD_PRELOAD=`pwd`/libmalloc.so ./test1

check_t:	libmalloc.so t-test1
	LD_PRELOAD=`pwd`/libmalloc.so ./t-test1

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar
