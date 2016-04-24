#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>

static pthread_mutex_t taskLock;
static pthread_mutex_t outputLock;
static sem_t taskSem;
static sem_t outputSem;

double compute(int x, int n);
void *WorkThread();

struct node {
	int x;
	int n;
	double res;
	pthread_t tid;
	struct node *next;
} taskQHead, outputQHead, *tmpNode;

int main(int argc, char *argv[])
{
	int ret;
	double res = 0;
	void *status;
	if (argc != 7) {
		printf("Usage: ./mtexponential -x X -n N -t T\n");
		return 0;
	}
	int x = atoi(argv[2]);
	int n = atoi(argv[4]);
	int t = atoi(argv[6]);
	int i;

	// init semaphore
	ret = pthread_mutex_init(&taskLock, NULL);
	ret = pthread_mutex_init(&outputLock, NULL);
	ret = sem_init(&taskSem, 0, 0);
	/* ret = sem_init(&taskSem, 0, -1); */
	ret = sem_init(&outputSem, 0, 0);
	/* ret = sem_init(&outputSem, 0, -1); */
	
	// create thread pool
	pthread_t pool[t];
	for (i = 0; i < t; i++) {
		ret = pthread_create(&pool[i], NULL, WorkThread, NULL);
	}

	// init queues
	/* struct node taskHead; */
	/* taskQHead = &taskHead; */
	/* struct node outputHead; */
	/* outputQHead = &outputHead; */
	
	// create task queue
	for (i = 0; i <= n; i++) {
		pthread_mutex_lock(&taskLock);
		tmpNode = (struct node *)malloc(sizeof(struct node));
		tmpNode->x = x;
		tmpNode->n = i;
		tmpNode->next = NULL;
		taskQHead.next = tmpNode;
		/* struct node newTask; */
		/* newTask.x = x; */
		/* newTask.n = i; */
		/* newTask.next = taskQHead.next; */
		/* taskQHead.next = &newTask; */
		pthread_mutex_unlock(&taskLock);
		sem_post(&taskSem);
	}
		
	
	// print out all results
	for (i = 0; i <= n; i++) {
		sem_wait(&outputSem);
		pthread_mutex_lock(&outputLock);
		struct node *cOutput = outputQHead.next;
		outputQHead.next = outputQHead.next->next;
		pthread_mutex_unlock(&outputLock);
		res += cOutput->res;
		printf("%d^%d / %d! : %f. From thread %zu\n", x, cOutput->n, cOutput->n, cOutput->res, cOutput->tid);
		free(cOutput);
	}

	// kill all threads
	for (i = 0; i < t; i++) {
		/* pthread_kill(pool[i], 9); */
		pthread_cancel(pool[i]);
		pthread_join(pool[i], &status);
	}

	printf("SUM(%d^n / n!) = %f\n", x, res);

	pthread_exit(NULL);
}

void *WorkThread()
{
	while(1) {
		sem_wait(&taskSem);
		pthread_mutex_lock(&taskLock);
		struct node *cTask = taskQHead.next;
		if (cTask == NULL) {
			pthread_mutex_unlock(&taskLock);
			continue;
		}
		taskQHead.next = taskQHead.next->next;
		pthread_mutex_unlock(&taskLock);
		/* struct node cRes; */
		/* cRes.n = cTask->n; */
		/* cRes.x = cTask->x; */
		/* cRes.res = compute(cRes.x, cRes.n); */
		cTask->res = compute(cTask->x, cTask->n);
		cTask->tid = pthread_self();
		pthread_mutex_lock(&outputLock);
		cTask->next = outputQHead.next;
		outputQHead.next = cTask;
		/* cRes.next = outputQHead->next; */
		/* outputQHead->next = &cRes; */
		pthread_mutex_unlock(&outputLock);
		sem_post(&outputSem);
	}

}

double compute(int x, int n)
{
	if (n == 0) return 1;
	int n_f = 1;
	int i;

	for (i = 1; i <= n; i++) {
		n_f *= i;
	}

	double res = pow(x, n) / n_f;

	return res;
}


