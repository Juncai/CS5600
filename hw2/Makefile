all:	check

default: check
	
clean:
	rm -rf worker master 

worker:
	gcc -O0 -o worker worker.c -lm -Wall -g

master:
	gcc -O0 -o master master.c -Wall -g

compile: clean worker master

test_worker:
	./worker -x 2 -n 3

test_sequential:
	./master --worker_path ./worker --wait_mechanism sequential -x 2 -n 3

test_select:
	./master --worker_path ./worker --wait_mechanism select -x 2 -n 3

test_poll:
	./master --worker_path ./worker --wait_mechanism poll -x 2 -n 3

test_epoll:
	./master --worker_path ./worker --wait_mechanism epoll -x 2 -n 3

gdb:
	gdb --args ./restart myckpt

check:	clean rst
	(sleep 3 && kill -12 `pgrep -n hello` && sleep 2 && pkill -9 hello) & 
	./hello
	(sleep 2 &&  pkill -9 restart) &
	make res

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar
