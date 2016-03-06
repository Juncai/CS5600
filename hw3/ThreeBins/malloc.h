#include <stdio.h>
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
} ArenaInfo;


void *malloc(size_t size);
void free(void *ptr);
void malloc_stats();
