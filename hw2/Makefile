all:	check

default: check
	
clean:
	rm -rf worker master 

build: 
	gcc -O0 -o worker worker.c -lm -Wall -g
	gcc -O0 -o master master.c -Wall -g

test_worker: 
	./worker -x 2 -n 10

test_sequential: 
	./master --worker_path ./worker --wait_mechanism sequential -x 2 -n 10

test_select: 
	./master --worker_path ./worker --wait_mechanism select -x 2 -n 10

test_poll: 
	./master --worker_path ./worker --wait_mechanism poll -x 2 -n 10

test_epoll: 
	./master --worker_path ./worker --wait_mechanism epoll -x 2 -n 10

check:	clean build test_worker test_sequential test_select test_poll test_epoll

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar
