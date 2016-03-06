#include "malloc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <errno.h>


static const size_t MIN_SIZE = 8;
static const size_t MAX_SIZE = 512;
static const int PAGES_REQUESTED = 4;
static struct ArenaInfo *infoHead;
static size_t BIN_SIZES[NUM_OF_BINS] = {8, 64, 512};

__thread ArenaInfo info;
/* __thread MallocHeader *freeLists[NUM_OF_BINS]; */
/* __thread MallocHeader *usedLists[NUM_OF_BINS]; */
/* __thread MallocHeader *usedListBig; */
/* __thread int mallocCount = 0; */
/* __thread int freeCount = 0; */
pthread_mutex_t sbrkMutex; 


int sizeToBinNo(size_t s);
void *getFree(MallocHeader *freeHead);
void *getSpace(int b);
int requestSpaceFromHeap(int b);
void flEnqueue(int qInd, MallocHeader *newHead);
MallocHeader *flDequeue(int qInd);
void ulEnqueue(int qInd, MallocHeader *newHead);
int ulDequeue(int qInd, MallocHeader *hdrToRemove);
int isInQueue(int qInd, MallocHeader *hdr);
void initArenaInfo();
void printInfo(ArenaInfo *ai);
void reclaimResources();


/* void *calloc(size_t nmemb, size_t size) */
/* { */
/*   return NULL; */
/* } */
void initArenaInfo() 
{
	ArenaInfo *p = infoHead;
	info.next = NULL;
	if (p == NULL) {
		infoHead = &info;
	} else {
		while (p->next != NULL) {
			p = p->next;
		}
		p->next = &info;
	}
	info.pid = getpid();
	info.numOfBins = NUM_OF_BINS;
	info.init = 1;
}

void *malloc(size_t size)
{
	// reclaim the resources from other thread if it's a new process
	reclaimResources();
	// add arena info for the current thread to the process info queue
	if (info.init == 0) {
		initArenaInfo();
	}

	void *sPtr = NULL;
	size_t realSize = size + sizeof(MallocHeader);
	if (size > MAX_SIZE) {	// for large space
		sPtr = mmap(NULL, realSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		assert(sPtr != MAP_FAILED);
		MallocHeader *hdr = (MallocHeader*) sPtr;
		hdr->size = realSize;
		ulEnqueue(-1, hdr);
		info.mmapSpace += realSize;
		printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
			 __FILE__, __LINE__, size, realSize, sPtr);
	} else {	// handle small memory requests
		int binNo = sizeToBinNo(size);
		sPtr = getSpace(binNo);
		
		info.mallocCount[binNo]++;
		printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
			 __FILE__, __LINE__, size, ((MallocHeader*)sPtr)->size, sPtr);
	}
	return sPtr + sizeof(MallocHeader);
}

int sizeToBinNo(size_t s)
{
	int i;
	for (i = 0; i < NUM_OF_BINS; i++) {
		if (s <= BIN_SIZES[i]) {
			return i;
		}
	}
	return -1;
}

void *getSpace(int b)
{
	MallocHeader *hdr = flDequeue(b);
	if (hdr == NULL) {
		requestSpaceFromHeap(b);
		hdr = flDequeue(b);
	}
	if (hdr != NULL) {
		ulEnqueue(b, hdr);
		info.sizesOfFL[b]--;
		info.sizesOfUL[b]++;
	}
	return hdr;
}

int requestSpaceFromHeap(int b)
{
	// memory request with size equal page size
	// Assume that, the page size will be 4k or multiple of 4k
	size_t pageSize = sysconf(_SC_PAGESIZE);
	size_t requestSize = pageSize * PAGES_REQUESTED;
	size_t nodeSize = BIN_SIZES[b] + sizeof(MallocHeader);
	int numOfNewNodes = requestSize / nodeSize;
	
	// need to acquire the mutex before sbrk
	int ret = pthread_mutex_lock(&sbrkMutex);
	if (ret == EINVAL) {
		pthread_mutex_init(&sbrkMutex, NULL);
		pthread_mutex_lock(&sbrkMutex);
	}
	void *sPtr = sbrk((intptr_t)requestSize);
	// release the mutex
	pthread_mutex_unlock(&sbrkMutex);

	if (sPtr == (void *)-1) return 1;

	MallocHeader *hdr = (MallocHeader*)sPtr;
	int i;
	for (i = 0; i < numOfNewNodes; i++) {
		hdr->size = nodeSize;
		flEnqueue(b, hdr);
		/* hdr += nodeSize; */
		hdr += nodeSize / 16;
	}
	info.sbrkSpace += requestSize;
	info.sizesOfFL[b] += numOfNewNodes;
	info.totalBlocks[b] += numOfNewNodes;
	printf("%d free slots of %zu BYTE are created!\n", numOfNewNodes, nodeSize);

	return 0;
}

void free(void *ptr)
{
	// reclaim the resources from other thread if it's a new process
	reclaimResources();

	MallocHeader *hdr = ptr - sizeof(MallocHeader);
	
	size_t realSize = hdr->size;
	size_t size = realSize - sizeof(MallocHeader);
	int binInd = sizeToBinNo(size);
	// TODO add the address to the free list of the thread
	if (size > MAX_SIZE) {
		ulDequeue(-1, hdr);
		munmap((void *)hdr, realSize);
		info.mmapSpace -= realSize;
	} else {
		ulDequeue(binInd, hdr);
		flEnqueue(binInd, hdr);
		info.freeCount[binInd]++;
		info.sizesOfUL[binInd]--;
		info.sizesOfFL[binInd]++;
	}

	printf("%s:%d free(%p): Freeing %zu bytes from %p\n",
		 __FILE__, __LINE__, ptr, realSize, hdr);
}

void flEnqueue(int qInd, MallocHeader *newHead)
{
	newHead->next = info.freeLists[qInd];
	info.freeLists[qInd] = newHead;
}

MallocHeader *flDequeue(int qInd)
{
	MallocHeader *res = info.freeLists[qInd];
	if (res != NULL) {
		info.freeLists[qInd] = res->next;
	}
	return res;
}

void ulEnqueue(int qInd, MallocHeader *newHead)
{
	if (qInd == -1) { // for big space
		newHead->next = info.usedListBig;
		info.usedListBig = newHead;
	} else {
		newHead->next = info.usedLists[qInd];
		info.usedLists[qInd] = newHead;
	}
}

int ulDequeue(int qInd, MallocHeader *hdrToRemove)
{
	MallocHeader *h;
	if (qInd == -1) { // for big space
		h = info.usedListBig;
		if (h == hdrToRemove) {
			info.usedListBig = hdrToRemove->next;
			return 0;
		}
	} else {
		h = info.usedLists[qInd];
		if (h == hdrToRemove) {
			info.usedLists[qInd] = hdrToRemove->next;
			return 0;
		}
	}

	while (h != NULL) {
		if (h->next == hdrToRemove) {
			h->next = hdrToRemove->next;
			return 0;
		}
	}
	return 1;
}
void malloc_stats()
{
	ArenaInfo *p = infoHead;
	int ind = 0;
	while (p != NULL) {
		printf("Arena %d:\n", ind++);
		printInfo(p);
		printf("##########################################\n");
		p = p->next;
	}
}

void printInfo(ArenaInfo *ai)
{
	printf("Space allocated with 'sbrk': %zu\n", ai->sbrkSpace);
	printf("Space allocated with 'mmap': %zu\n", ai->mmapSpace);
	printf("Total number of bins:        %d\n", ai->numOfBins);
	int i;
	for (i = 0; i < ai->numOfBins; i++) {
		printf("------------------------------------------\n");
		printf("Bin %d with %zu-byte block size: \n", i, BIN_SIZES[i]);
		printf("Total number of blocks:      %zu\n", ai->totalBlocks[i]);
		printf("Used blocks:                 %zu\n", ai->sizesOfUL[i]);
		printf("Free blocks:                 %zu\n", ai->sizesOfFL[i]);
		printf("Total allocation requests:   %zu\n", ai->mallocCount[i]);
		printf("Total free requests:         %zu\n", ai->freeCount[i]);
	}
}

void reclaimResources()
{
	pid_t cPid = getpid();
	if (cPid == infoHead->pid) {
		return;
	}

	ArenaInfo *p = infoHead;
	MallocHeader *hdr;
	int i;
	while (p != NULL) {
		if (p != &info) {
			// reclaim memory in BINs
			for (i = 0; i < p->numOfBins; i++) {
				hdr = p->freeLists[i];
				while (hdr != NULL) {
					flEnqueue(i, hdr);
				}
				hdr = p->usedLists[i];
				while (hdr != NULL) {
					flEnqueue(i, hdr);
				}
				info.sizesOfFL[i] += p->sizesOfFL[i];
				info.sizesOfUL[i] += p->sizesOfUL[i];
				info.totalBlocks[i] += p->totalBlocks[i];
			}
			// TODO reclaim big memory block
			// TODO summing the total resources allocated
		}
	}
}
			
/* void *realloc(void *ptr, size_t size) */
/* { */
/*   // Allocate new memory (if needed) and copy the bits from old location to new. */

/*   return NULL; */
/* } */
