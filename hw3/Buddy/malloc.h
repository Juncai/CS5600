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
	long totalBlocks[NUM_OF_BINS];
	long sizesOfFL[NUM_OF_BINS];
	long sizesOfUL[NUM_OF_BINS];
	long mallocCount[NUM_OF_BINS];
	long freeCount[NUM_OF_BINS];
	size_t sbrkSpace;
	size_t mmapSpace;
	struct ArenaInfo *next;
	int init;
	pid_t pid;
	pthread_t tid;
} ArenaInfo;


void *malloc(size_t size);
void free(void *ptr);
void malloc_stats();
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
