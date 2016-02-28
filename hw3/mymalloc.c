#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#define MIN_SIZE 32
#define MAX_SIZE 4096
#define PAGES_REQUESTED 4
#define NUM_OF_BINS 8

static int BIN_SIZES[NUM_OF_BINS] = {32, 64, 128, 256, 512, 1024, 2048, 4096};

typedef struct MallocHeader
{
	size_t size;
	struct MallocHeader *next;
} MallocHeader;

__thread MallocHeader *freeLists[NUM_OF_BINS];
__thread MallocHeader *usedLists[NUM_OF_BINS];
/* __thread MallocHeader *freeListBig; */
__thread MallocHeader *usedListBig;
__thread int mallocCount = 0;
__thread int freeCount = 0;
pthread_mutex_t sbrkMutex; 


void *mymalloc(size_t size);
void myfree(void *ptr);
void *getFree(MallocHeader *freeHead);
int getBinIndex(size_t s);
void *getSpace(size_t s);
int requestFromHeap();
void flEnqueue(int qInd, MallocHeader *newHead);
MallocHeader *flDequeue(int qInd);
void ulEnqueue(int qInd, MallocHeader *newHead);
int ulDequeue(int qInd, MallocHeader *hdrToRemove);
void transferSpace(int sInd, int dInd);
int isInQueue(int qInd, MallocHeader *hdr);
void myfreeHelper(int qInd, MallocHeader *tar);
int removeFromFL(int qInd, MallocHeader *tar);
MallocHeader *getBuddyAddr(MallocHeader *hdr);

int main() {
	long pageSize = sysconf(_SC_PAGESIZE);
	printf("Pagesize: %ld\n", pageSize);

	char *str;
	str = (char *)mymalloc(640);
	strcpy(str, "haha");
	printf("%s\n", str);
	myfree(str);

	// should reuse the previously free slot
	str = (char *)mymalloc(640);
	strcpy(str, "haha");
	printf("%s\n", str);
	myfree(str);

	return(0);
}


/* void *calloc(size_t nmemb, size_t size) */
/* { */
/*   return NULL; */
/* } */

void *mymalloc(size_t size)
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
	int flIndex = getBinIndex(s);
	int i;
	MallocHeader *hdr;
	for (i = flIndex; i < NUM_OF_BINS; i++) {
		transferSpace(i, flIndex);
		hdr = flDequeue(flIndex);
		if (hdr != NULL) {
			ulEnqueue(flIndex, hdr);
			break;
		}
	}
	return hdr;
}

void transferSpace(int sInd, int dInd) 
{
	if (freeLists[sInd] == NULL) return;
	if (dInd == sInd) return;

	size_t newSize = BIN_SIZES[sInd - 1];
	MallocHeader *hdr0 = flDequeue(sInd);
	MallocHeader *hdr1 = hdr0 + newSize;
	hdr0->size = newSize;
	hdr1->size = newSize;
	flEnqueue(sInd - 1, hdr0);
	flEnqueue(sInd - 1, hdr1);
	transferSpace(sInd - 1, dInd);
}

int requestFromHeap()
{
	// TODO memory request with size equal page size
	// Assume that, the page size will be 4k or multiple of 4k
	size_t pageSize = sysconf(_SC_PAGESIZE);
	size_t requestSize = pageSize * PAGES_REQUESTED;
	int numOfMaxSize = requestSize / MAX_SIZE;
	
	void *sPtr = sbrk((intptr_t)requestSize);
	if (sPtr == (void *)-1) return 1;

	MallocHeader *hdr = (MallocHeader*)sPtr;
	int i;
	for (i = 0; i < numOfMaxSize; i++) {
		hdr->size = MAX_SIZE;
		flEnqueue(NUM_OF_BINS - 1, hdr);
		hdr += MAX_SIZE;
	}

	return 0;
}

int getBinIndex(size_t s)
{
	int i;
	for (i = 0; i < 8; i++) {
		if ((int)s <= BIN_SIZES[i]) {
			return i;
		}
	}
	return -1;
}

void myfree(void *ptr)
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

