#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/wait.h>
#include <pthread.h>

void *workThread(void *tno);

static void *p;

int main(int argc, char **argv)
{
	size_t size = 12;
	/* void *mem = malloc(size); */
	p = malloc(size);
	printf("Successfully malloc'd %zu bytes at addr %p\n", size, p);
	assert(p != NULL);
	malloc_stats();

	// create some threads
	int numOfT = 1;
	pthread_t threads[numOfT];
	int ret;
	long t;
	void *status;
	for (t = 0; t < numOfT; t++) {
		ret = pthread_create(&threads[t], NULL, workThread, (void *)t);
		if (ret) {
			exit(-1);
		}
		pthread_join(threads[t], &status);
	}
	/* free(mem); */
	/* printf("Successfully free'd %zu bytes from addr %p\n", size, mem); */
	malloc_stats();

	return 0;
}

void *workThread(void *tno)
{
	long tn = (long)tno;
	/* size_t s = 6; */
	printf("Thread NO: %ld\n", tn);
	/* malloc_stats(); */
	/* void *tmem = malloc(s); */
	/* printf("Thread %ld: Successfully malloc'd %zu bytes at addr %p\n", tn, s, tmem); */
	/* assert(tmem != NULL); */
	malloc_stats();
	size_t size = 12;
	void *mem = malloc(size);
	int status = 0;
	printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
	assert(mem != NULL);
	malloc_stats();

	// new process
	int cid = fork();
	if (cid == 0) {
		printf("Child process:\n");
		malloc_stats();
		void *cmem = malloc(size);
		printf("Child: Successfully malloc'd %zu bytes at addr %p\n", size, cmem);
		assert(cmem != NULL);
		malloc_stats();
		free(cmem);
		printf("Child: Successfully free'd %zu bytes from addr %p\n", size, cmem);
		malloc_stats();
		exit(0);
	} else {
		waitpid(cid, &status, 0);
	}

	free(mem);
	printf("Successfully free'd %zu bytes from addr %p\n", size, mem);
	malloc_stats();
	// double free the memory
	free(mem);
	printf("Successfully double free'd %zu bytes from addr %p\n", size, mem);
	malloc_stats();


	free(p);
	printf("Successfully free'd %zu bytes from addr %p from the main thread\n", size, p);
	malloc_stats();

	pthread_exit(NULL);
}
