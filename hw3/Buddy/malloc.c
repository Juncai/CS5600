#include "malloc.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <signal.h>

#ifndef TEST
#define TEST 0
#endif

static const size_t MIN_SIZE = 8;
static const size_t MAX_SIZE = 512;
static const int PAGES_REQUESTED = 128;
/* static struct ArenaInfo *infoHead = NULL; */
static struct ArenaInfo centralArena;
static int centralInitiated = 0;
static size_t BIN_SIZES[NUM_OF_BINS] = {8, 64, 512};
static int BLOCKS_REQUESTED[NUM_OF_BINS] = {682, 204, 31};

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static pthread_mutex_t *centralLock;
static pthread_mutex_t *sbrkLock;
/* __thread pthread_mutex_t infoLock = PTHREAD_MUTEX_INITIALIZER; */
__thread ArenaInfo *info;

int sizeToBinNo(size_t s);
void *getFree(MallocHeader *freeHead);
void *getSpace(int b);
static int requestSpaceFromHeap(int b);
static void flEnqueue(ArenaInfo *ai, int qInd, MallocHeader *newHead);
MallocHeader *flDequeue(int qInd);
void ulEnqueue(int qInd, MallocHeader *newHead);
int ulDequeue(int qInd, MallocHeader *hdrToRemove);
int isInQueue(int qInd, MallocHeader *hdr);
void initArenaInfo();
void printInfo(ArenaInfo *ai);
static void reclaimResources();
static void reclaimResourcesHelper(ArenaInfo *src);
static int isInfoAlive(ArenaInfo *ai);
int requestSpaceFromCentral(int b);

static void prepare()
{
	pthread_mutex_lock(centralLock);
	pthread_mutex_lock(sbrkLock);
}

static void parent()
{
	pthread_mutex_unlock(centralLock);
	pthread_mutex_unlock(sbrkLock);
}

static void child()
{
	pthread_once_t once = PTHREAD_ONCE_INIT;
	memcpy(&once_control, &once, sizeof(once_control));
	// correct the pid and tid of current thread
	info->pid = getpid();
	info->tid = pthread_self();
}

static void mutexInit()
{
	// optimize the performance by combine the sbrk calls
	centralLock = (pthread_mutex_t *) sbrk((intptr_t)sizeof(pthread_mutex_t));
	sbrkLock = (pthread_mutex_t *) sbrk((intptr_t)sizeof(pthread_mutex_t));
	pthread_mutex_init(centralLock, NULL);
	pthread_mutex_init(sbrkLock, NULL);
}

static void flEnqueue(ArenaInfo *ai, int qInd, MallocHeader *newHead)
{
	newHead->next = ai->freeLists[qInd];
	ai->freeLists[qInd] = newHead;
}

static int isInfoAlive(ArenaInfo *ai)
{
	pid_t cPid = getpid();
	if (ai->pid != cPid || ai->tid == 0) return 0;
	int ret = pthread_kill(ai->tid, 0);
	if (ret == ESRCH) return 0;
	return 1;
}

static void reclaimResourcesHelper(ArenaInfo *src)
{
	// reclaim memory in BINs
	MallocHeader *tail;
	int i;
	for (i = 0; i < NUM_OF_BINS; i++) {
		if (src->sizesOfFL[i] > 0) {
			tail = src->freeLists[i];
			while (tail->next != NULL) {
				tail = tail->next;
			}
			tail->next = centralArena.freeLists[i];
			centralArena.freeLists[i] = src->freeLists[i];
			centralArena.sizesOfFL[i] += src->sizesOfFL[i];
		}
		if (src->sizesOfUL[i] > 0) {
			tail = src->usedLists[i];
			while (tail->next != NULL) {
				tail = tail->next;
			}
			tail->next = centralArena.freeLists[i];
			centralArena.freeLists[i] = src->usedLists[i];
			centralArena.sizesOfFL[i] += src->sizesOfUL[i];
		}

		centralArena.totalBlocks[i] += src->totalBlocks[i];
	}

	// reclaim big memory block
	MallocHeader *next;
	tail = src->usedListBig;
	while (tail != NULL) {
		next = tail->next;
		munmap((void *)tail, tail->size);
		tail = next;
	}
}
	
static void reclaimResources()
{
	ArenaInfo *ai = centralArena.next;
	ArenaInfo *prev = &centralArena;
	while (ai != NULL) {
		if (!isInfoAlive(ai)) {
			prev->next = ai->next;
			reclaimResourcesHelper(ai);
			ai = prev->next;
		} else {
			prev = ai;
			ai = ai->next;
		}
	}
}

static int requestSpaceFromHeap(int b)
{
	// first try to reclaim resources from exited threads
	reclaimResources();
	// memory request with size equal page size
	// Assume that, the page size will be 4k or multiple of 4k
	size_t pageSize = sysconf(_SC_PAGESIZE);
	size_t requestSize = pageSize * PAGES_REQUESTED;
	size_t nodeSize = BIN_SIZES[b] + sizeof(MallocHeader);
	int numOfNewNodes = requestSize / nodeSize;
	
	// need to acquire the mutex before sbrk
	pthread_mutex_lock(sbrkLock);
	void *sPtr = sbrk((intptr_t)requestSize);
	// release the mutex
	pthread_mutex_unlock(sbrkLock);

	// handle sbrk failure
	if (sPtr == (void *)-1) {
		errno = ENOMEM;
		return 1;
	}

	MallocHeader *hdr = (MallocHeader*)sPtr;
	int i;
	for (i = 0; i < numOfNewNodes; i++) {
		hdr->size = nodeSize;
		flEnqueue(&centralArena, b, hdr);
		/* hdr += nodeSize; */
		hdr += nodeSize / 16;
	}
	centralArena.sbrkSpace += requestSize;
	centralArena.sizesOfFL[b] += numOfNewNodes;
	centralArena.totalBlocks[b] += numOfNewNodes;
#if TEST > 0
	printf("%d free slots of %zu BYTE are created!\n", numOfNewNodes, nodeSize);
#endif

	return 0;
}

static void centralArenaInit()
{
	// pre allocate some space in the central arena
	pthread_mutex_lock(centralLock);
	int i;
	for (i = 0; i < NUM_OF_BINS; i++) {
		requestSpaceFromHeap(i);
	}
	centralInitiated = 1;
	pthread_mutex_unlock(centralLock);
}

static void init()
{
	mutexInit();
	if (centralInitiated) {
		// reclaim resources from other thread
		reclaimResources();
	} else {
		centralArenaInit();
	}
	// handling fork issues
	// initialize pthread_atfork
	pthread_atfork(&prepare, &parent, &child);
}

int requestSpaceFromCentral(int b)
{
	pthread_mutex_lock(centralLock);
	int ret = 0;
	if (centralArena.sizesOfFL[b] < BLOCKS_REQUESTED[b]) {
		ret = requestSpaceFromHeap(b);
	}
	if (ret == 1) return ret;
	int count = BLOCKS_REQUESTED[b];
	MallocHeader *head = centralArena.freeLists[b];
	MallocHeader *tail = centralArena.freeLists[b];
	while (count-- > 1) {
		tail = tail->next;
	}
	centralArena.freeLists[b] = tail->next;
	centralArena.sizesOfFL[b] -= BLOCKS_REQUESTED[b];
	centralArena.totalBlocks[b] -= BLOCKS_REQUESTED[b];
	pthread_mutex_unlock(centralLock);
	tail->next = info->freeLists[b];
	info->freeLists[b] = head;
	info->sizesOfFL[b] += BLOCKS_REQUESTED[b];
	info->totalBlocks[b] += BLOCKS_REQUESTED[b];
	info->sbrkSpace += BLOCKS_REQUESTED[b] * BIN_SIZES[b];
	return 0;
}

void initArenaInfo() 
{
	/* if (info == centralArena.next) { */
	/* 	return; */
	/* } */
	// request some space from the heap for arena info, then initialize
	pthread_mutex_lock(sbrkLock);
	/* info = (ArenaInfo *) sbrk((intptr_t) sysconf(_SC_PAGESIZE)); */
	info = (ArenaInfo *) sbrk((intptr_t) sizeof(ArenaInfo));
	pthread_mutex_unlock(sbrkLock);
	memset(info, 0, sizeof(ArenaInfo));
	
	// request some space from central arena
	int i;
	for (i = 0; i < NUM_OF_BINS; i++) {
		requestSpaceFromCentral(i);
	}

	info->pid = getpid();
	info->tid = pthread_self();	
	info->numOfBins = NUM_OF_BINS;

	// need to acquire the mutex before modify the process's info head
	pthread_mutex_lock(centralLock);
	// HACK: loop through all the existing thread arenas,
	//       replace the same tid with NULL
	ArenaInfo *p = centralArena.next;
	while(p != NULL) {
		if (pthread_equal(info->tid, p->tid)) {
		/* if (info->tid == p->tid) { */
			p->tid = 0;
		}
		p = p->next;
	}
	reclaimResources();

	// add current arena to the central arena
	info->next = centralArena.next;
	centralArena.next = info;

	pthread_mutex_unlock(centralLock);
	/* info->init = 1; */
}

void *malloc(size_t size)
{
	// add arena info for the current thread to the process info queue
	pthread_once(&once_control, &init);
	/* if (info->init == 0) { */
	if (info == NULL) {
		initArenaInfo();
	}

	void *sPtr = NULL;
	size_t realSize = size + sizeof(MallocHeader);
	if (size > MAX_SIZE) {	// for large space
		sPtr = mmap(NULL, realSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		// handle MAP_FAILED
		if (sPtr == MAP_FAILED) return NULL;
		MallocHeader *hdr = (MallocHeader*) sPtr;
		hdr->size = realSize;
		ulEnqueue(-1, hdr);
		info->mmapSpace += realSize;
#if TEST > 0
		printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
			 __FILE__, __LINE__, size, realSize, sPtr);
#endif
	} else {	// handle small memory requests
		int binNo = sizeToBinNo(size);
		info->mallocCount[binNo]++;
		sPtr = getSpace(binNo);
		if (sPtr == NULL) return NULL;
		
#if TEST > 0
		printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
			 __FILE__, __LINE__, size, ((MallocHeader*)sPtr)->size, sPtr);
#endif
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
		// try to request space from central
		requestSpaceFromCentral(b);
		hdr = flDequeue(b);
	}

	if (hdr != NULL) {
		ulEnqueue(b, hdr);
		info->sizesOfFL[b]--;
		info->sizesOfUL[b]++;
	}
	return hdr;
}

void free(void *ptr)
{
	// add arena info for the current thread to the process info queue
	pthread_once(&once_control, &init);
	/* if (info->init == 0) { */
	if (info == NULL) {
		initArenaInfo();
	}

	if (ptr == NULL) return;

	MallocHeader *hdr = ptr - sizeof(MallocHeader);
	
	size_t realSize = hdr->size;
	size_t size = realSize - sizeof(MallocHeader);
	int binInd = sizeToBinNo(size);

	if (size > MAX_SIZE) {
		ulDequeue(-1, hdr);
		munmap((void *)hdr, realSize);
		/* info.mmapSpace -= realSize; */
	} else {
		ulDequeue(binInd, hdr);
		flEnqueue(info, binInd, hdr);
		info->freeCount[binInd]++;
		info->sizesOfUL[binInd]--;
		info->sizesOfFL[binInd]++;
	}

#if TEST > 0
	printf("%s:%d free(%p): Freeing %zu bytes from %p\n",
		 __FILE__, __LINE__, ptr, realSize, hdr);
#endif
}


MallocHeader *flDequeue(int qInd)
{
	MallocHeader *res = info->freeLists[qInd];
	if (res != NULL) {
		info->freeLists[qInd] = res->next;
	}
	return res;
}

void ulEnqueue(int qInd, MallocHeader *newHead)
{
	if (qInd == -1) { // for big space
		newHead->next = info->usedListBig;
		info->usedListBig = newHead;
	} else {
		newHead->next = info->usedLists[qInd];
		info->usedLists[qInd] = newHead;
	}
}

int ulDequeue(int qInd, MallocHeader *hdrToRemove)
{
	/* pthread_mutex_lock(&info.infoLock); */
	MallocHeader *h;
	if (qInd == -1) { // for big space
		h = info->usedListBig;
		if (h == hdrToRemove) {
			info->usedListBig = hdrToRemove->next;
			/* pthread_mutex_unlock(&info.infoLock); */
			return 0;
		}
	} else {
		h = info->usedLists[qInd];
		if (h == hdrToRemove) {
			info->usedLists[qInd] = hdrToRemove->next;
			/* pthread_mutex_unlock(&info.infoLock); */
			return 0;
		}
	}

	while (h != NULL) {
		if (h->next == hdrToRemove) {
			h->next = hdrToRemove->next;
			/* pthread_mutex_unlock(&info.infoLock); */
			return 0;
		}
		h = h->next;
	}
	/* pthread_mutex_unlock(&info.infoLock); */
	return 1;
}

void malloc_stats()
{
	ArenaInfo *p = centralArena.next;
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
	printf("PID: %d\n", ai->pid);
	printf("TID: %zu\n", ai->tid);
	printf("Space allocated with 'sbrk': %zu\n", ai->sbrkSpace);
	printf("Space allocated with 'mmap': %zu\n", ai->mmapSpace);
	printf("Total number of bins:        %d\n", ai->numOfBins);
	int i;
	for (i = 0; i < ai->numOfBins; i++) {
		printf("------------------------------------------\n");
		printf("Bin %d with %zu-byte block size: \n", i, BIN_SIZES[i]);
		printf("Total number of blocks:      %d\n", ai->totalBlocks[i]);
		printf("Used blocks:                 %d\n", ai->sizesOfUL[i]);
		printf("Free blocks:                 %d\n", ai->sizesOfFL[i]);
		printf("Total allocation requests:   %d\n", ai->mallocCount[i]);
		printf("Total free requests:         %d\n", ai->freeCount[i]);
	}
}

		
void *realloc(void *ptr, size_t size)
{
	// add arena info for the current thread to the process info queue
	pthread_once(&once_control, &init);
	/* if (info->init == 0) { */
	if (info == NULL) {
		initArenaInfo();
	}

	// Allocate new memory (if needed) and copy the bits from old location to new.
	if (ptr == NULL) {
		return malloc(size);
	}
	// If the original block is large enough, return it.
    MallocHeader *hdr = ptr - sizeof(MallocHeader);
    size_t actualSize = hdr->size - sizeof(MallocHeader);
    if (size < actualSize) {
		return ptr;
    }

	// Allocate new space
	void *newPtr = malloc(size);
	memcpy(newPtr, ptr, actualSize);
	// free the old space
	free(ptr);
	
    return newPtr;
}

void *calloc(size_t nmemb, size_t size)
{
	// add arena info for the current thread to the process info queue
	pthread_once(&once_control, &init);
	/* if (info->init == 0) { */
	if (info == NULL) {
		initArenaInfo();
	}

	if (nmemb == 0 || size == 0) {
		return NULL;
	}

	// allocate space for multiple items
	size_t totalSize = size * nmemb;
	void *p = malloc(totalSize);
	memset(p, 0, totalSize);
	return p;
}
