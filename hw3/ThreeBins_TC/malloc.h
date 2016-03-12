/* Author: Jun Cai */
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>

#define NUM_OF_BINS 3

typedef struct MallocHeader
{
	size_t size;
	struct MallocHeader *next;
} MallocHeader;

typedef struct ArenaInfo
{
	int numOfBins;
	struct MallocHeader *freeLists[NUM_OF_BINS];
	struct MallocHeader *usedLists[NUM_OF_BINS];
	struct MallocHeader *usedListBig;
	int totalBlocks[NUM_OF_BINS];
	int sizesOfFL[NUM_OF_BINS];
	int sizesOfUL[NUM_OF_BINS];
	int mallocCount[NUM_OF_BINS];
	int freeCount[NUM_OF_BINS];
	size_t sbrkSpace;
	size_t mmapSpace;
	struct ArenaInfo *next;
	/* pthread_mutex_t infoLock; */
	/* int init; */
	pid_t pid;
	pthread_t tid;
} ArenaInfo;


void *malloc(size_t size);
void free(void *ptr);
void malloc_stats();
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
void *memalign(size_t alignment, size_t size);
