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

static size_t BIN_SIZES[NUM_OF_BINS] = {32, 64, 128, 256, 512, 1024, 2048, 4096};

__thread MallocHeader *freeLists[NUM_OF_BINS];
__thread MallocHeader *usedLists[NUM_OF_BINS];
/* __thread MallocHeader *freeListBig; */
__thread MallocHeader *usedListBig;
__thread int mallocCount = 0;
__thread int freeCount = 0;
pthread_mutex_t sbrkMutex; 


void *getFree(MallocHeader *freeHead);
int getBinIndex(size_t s);
void *getSpace(size_t s);
int requestSpaceFromHeap();
void flEnqueue(int qInd, MallocHeader *newHead);
MallocHeader *flDequeue(int qInd);
void ulEnqueue(int qInd, MallocHeader *newHead);
int ulDequeue(int qInd, MallocHeader *hdrToRemove);
void transferSpace(int sInd, int dInd);
int isInQueue(int qInd, MallocHeader *hdr);
void myfreeHelper(int qInd, MallocHeader *tar);
int removeFromFL(int qInd, MallocHeader *tar);
MallocHeader *getBuddyAddr(MallocHeader *hdr);


/* void *calloc(size_t nmemb, size_t size) */
/* { */
/*   return NULL; */
/* } */

void *malloc(size_t size)
{
	mallocCount++;
	void *sPtr = NULL;
	size_t realSize = size + sizeof(MallocHeader);
	if (realSize > MAX_SIZE) {	// for large space
		sPtr = mmap(NULL, realSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		assert(sPtr != MAP_FAILED);
		MallocHeader *hdr = (MallocHeader*) sPtr;
		hdr->size = realSize;
		ulEnqueue(-1, hdr);
		printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
			 __FILE__, __LINE__, size, realSize, sPtr);
	} else {	// handle small memory requests
		if (realSize <= MIN_SIZE) {
			realSize = MIN_SIZE;
		} 
		sPtr = getSpace(realSize);
		
		printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
			 __FILE__, __LINE__, size, ((MallocHeader*)sPtr)->size, sPtr);
	}
	return sPtr + sizeof(MallocHeader);
}

void *getSpace(size_t s)
{
	int binIndex = getBinIndex(s);
	int i;
	MallocHeader *hdr;
	for (i = binIndex; i < NUM_OF_BINS; i++) {
		transferSpace(i, binIndex);
		hdr = flDequeue(binIndex);
		if (hdr != NULL) {
			ulEnqueue(binIndex, hdr);
			break;
		}
	}
	return hdr;
}

void transferSpace(int sInd, int dInd) 
{
	if (freeLists[sInd] == NULL) {

		if (sInd == NUM_OF_BINS - 1) {
			requestSpaceFromHeap();
		} else {
			return;
		}
	} 

	if (dInd == sInd) return;

	size_t newSize = BIN_SIZES[sInd - 1];
	MallocHeader *hdr0 = flDequeue(sInd);
	MallocHeader *hdr1 = hdr0 + newSize / 16;
	hdr0->size = newSize;
	hdr1->size = newSize;
	printf("Two %zu BYTE slots created.\n", newSize);
	flEnqueue(sInd - 1, hdr0);
	flEnqueue(sInd - 1, hdr1);
	transferSpace(sInd - 1, dInd);
}

int requestSpaceFromHeap()
{
	// memory request with size equal page size
	// Assume that, the page size will be 4k or multiple of 4k
	size_t pageSize = sysconf(_SC_PAGESIZE);
	size_t requestSize = pageSize * PAGES_REQUESTED;
	int numOfMaxSize = requestSize / MAX_SIZE;
	
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
	for (i = 0; i < numOfMaxSize; i++) {
		hdr->size = MAX_SIZE;
		flEnqueue(NUM_OF_BINS - 1, hdr);
		hdr += MAX_SIZE / 16;
	}
	printf("%d free slots of %d BYTE are created!\n", numOfMaxSize, MAX_SIZE);

	return 0;
}

int getBinIndex(size_t s)
{
	int i;
	for (i = 0; i < 8; i++) {
		if (s <= BIN_SIZES[i]) {
			return i;
		}
	}
	return -1;
}

void free(void *ptr)
{
	freeCount++;
	MallocHeader *hdr = ptr - sizeof(MallocHeader);
	
	size_t realSize = hdr->size;
	int binInd;
	// TODO add the address to the free list of the thread
	if (realSize > MAX_SIZE) {
		ulDequeue(-1, hdr);
		munmap((void *)hdr, realSize);
	} else {
		binInd = getBinIndex(realSize);
		ulDequeue(binInd, hdr);
		myfreeHelper(binInd, hdr);
	}

	printf("%s:%d free(%p): Freeing %zu bytes from %p\n",
		 __FILE__, __LINE__, ptr, realSize, hdr);
}

void myfreeHelper(int qInd, MallocHeader *tar)
{
	if (qInd == NUM_OF_BINS - 1) {
		flEnqueue(qInd, tar);
		return;
	}

	MallocHeader *buddy = getBuddyAddr(tar);
	int ret = removeFromFL(qInd, buddy);
	if (ret != 0) {
		flEnqueue(qInd, tar);
		return;
	} else {	// if coalesce is possible
		MallocHeader *first;
		if (tar > buddy) {
			first = buddy;
		} else {
			first = tar;
		}
		first->size = BIN_SIZES[qInd + 1];
		printf("A %zu BYTE free slot is created.\n", BIN_SIZES[qInd + 1]);
		myfreeHelper(qInd + 1, first);
	}
}

MallocHeader *getBuddyAddr(MallocHeader *hdr)
{
	size_t s = hdr->size;
	return (MallocHeader *)((long)hdr ^ (int)s);
}

int removeFromFL(int qInd, MallocHeader *tar)
{
	MallocHeader *h = freeLists[qInd];
	if (h == tar) {
		freeLists[qInd] = tar->next;
		return 0;
	}
	while (h != NULL) {
		if (h->next == tar) {
			h->next = tar->next;
			return 0;
		}
		h = h->next;
	}
	return 1;
}

void flEnqueue(int qInd, MallocHeader *newHead)
{
	newHead->next = freeLists[qInd];
	freeLists[qInd] = newHead;
}

MallocHeader *flDequeue(int qInd)
{
	MallocHeader *res = freeLists[qInd];
	if (res != NULL) {
		freeLists[qInd] = res->next;
	}
	return res;
}

void ulEnqueue(int qInd, MallocHeader *newHead)
{
	if (qInd == -1) { // for big space
		newHead->next = usedListBig;
		usedListBig = newHead;
	} else {
		newHead->next = usedLists[qInd];
		usedLists[qInd] = newHead;
	}
}

int ulDequeue(int qInd, MallocHeader *hdrToRemove)
{
	MallocHeader *h;
	if (qInd == -1) { // for big space
		h = usedListBig;
		if (h == hdrToRemove) {
			usedListBig = hdrToRemove->next;
			return 0;
		}
	} else {
		h = usedLists[qInd];
		if (h == hdrToRemove) {
			usedLists[qInd] = hdrToRemove->next;
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
/* void *realloc(void *ptr, size_t size) */
/* { */
/*   // Allocate new memory (if needed) and copy the bits from old location to new. */

/*   return NULL; */
/* } */

