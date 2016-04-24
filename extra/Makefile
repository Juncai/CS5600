all:	check

default: check
	
clean:
	rm -rf mtexponential

build: 
	gcc -O0 mtexponential.c -lpthread -lm -g -o mtexponential 

test: 
	./mtexponential -x 2 -n 10 -t 4

check:	clean build test

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar
