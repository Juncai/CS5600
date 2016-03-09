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

#ifndef TEST
#define TEST 1
#endif


static const size_t MIN_SIZE = 8;
static const size_t MAX_SIZE = 512;
static const int PAGES_REQUESTED = 4;
static struct ArenaInfo *infoHead = NULL;
static size_t BIN_SIZES[NUM_OF_BINS] = {8, 64, 512};

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static pthread_mutex_t *sbrkLock;
static pthread_mutex_t *infoListLock;
/* __thread pthread_mutex_t infoLock = PTHREAD_MUTEX_INITIALIZER; */
__thread ArenaInfo *info = NULL;

int sizeToBinNo(size_t s);
void *getFree(MallocHeader *freeHead);
void *getSpace(int b);
int requestSpaceFromHeap(int b);
void flEnqueue(ArenaInfo *ai, int qInd, MallocHeader *newHead);
MallocHeader *flDequeue(int qInd);
void ulEnqueue(int qInd, MallocHeader *newHead);
int ulDequeue(int qInd, MallocHeader *hdrToRemove);
int isInQueue(int qInd, MallocHeader *hdr);
void initArenaInfo();
void printInfo(ArenaInfo *ai);
int reclaimResources();
void reclaimResourcesHelper(ArenaInfo *src, ArenaInfo *dest);

static void prepare()
{
	pthread_mutex_lock(infoListLock);
}

static void parent()
{
	pthread_mutex_unlock(infoListLock);
}

static void child()
{
	pthread_once_t once = PTHREAD_ONCE_INIT;
	memcpy(&once_control, &once, sizeof(once_control));
}

static void mutex_init()
{
	// optimize the performance by combine the sbrk calls
	infoListLock = (pthread_mutex_t *) sbrk((intptr_t)sizeof(pthread_mutex_t));
	pthread_mutex_init(infoListLock, NULL);
	sbrkLock = (pthread_mutex_t *) sbrk((intptr_t)sizeof(pthread_mutex_t));
	pthread_mutex_init(sbrkLock, NULL);
}

static void init()
{
	mutex_init();
	// handling fork issues
	// initialize pthread_atfork
	pthread_atfork(&prepare, &parent, &child);
}

static void cleanup_handler(void *arg)
{
	// 1. remove the entry from the thread's ArenoInfo list
	// 2. if there is any other threads in the process, 
	//    give all the resources to the first one on the list.
	
	// acquire info mutex 
	pthread_mutex_lock(infoListLock);
	// remove self from the info list
	ArenaInfo *p = infoHead;
	if (p == info) {
		infoHead = infoHead->next;
	} else {
		while (p->next != info) {
			p = p->next;
		}
		p->next = info->next;
	}

		// give sbrk space to other threads if any
	if (infoHead != NULL) {
		reclaimResourcesHelper(info, infoHead);
	} else {
		// free mmap space
		MallocHeader *hdr = info->usedListBig;
		MallocHeader *next;
		while (hdr != NULL) {
			next = hdr->next;
			munmap(hdr, hdr->size);
			hdr = next;
		}
	}

	pthread_mutex_unlock(infoListLock);
}

void initArenaInfo() 
{
	// this is called when a new thread is created
	if (info != NULL) {
		return;
	}
	// push pthread cleanup handler
	/* pthread_cleanup_push(&cleanup_handler, NULL); */
	
	/* pthread_cleanup_pop(1); */
	// request a page of space for the ArenaInfo for the current thread
	// acquire sbrk mutex to get the space
	pthread_mutex_lock(sbrkLock);
	/* info  = (ArenaInfo *) sbrk((intptr_t)sysconf(_SC_PAGESIZE)); */
	info  = (ArenaInfo *) sbrk((intptr_t) sizeof(ArenaInfo));
	pthread_mutex_unlock(sbrkLock);
	/* assert(info != NULL); */

	// initialize the ArenaInfo
	memset(info, 0, sizeof(ArenaInfo));
	/* info->next = NULL; */
	info->pid = getpid();
	info->tid = pthread_self();	
	info->numOfBins = NUM_OF_BINS;
	pthread_mutex_init(&info->infoLock, NULL);
	/* info->infoLock = &infoLock; */

	// try to reclaim resources from parent process
	/* int ret = reclaimResources(); */

	// need to acquire the mutex before modify the process's info head
	pthread_mutex_lock(infoListLock);

	/* if (ret) { */
		/* ArenaInfo *p = infoHead; */
	info->next = infoHead;
	infoHead = info;
	/* } */
	/* if (p == NULL) { */
	/* 	infoHead = info; */
	/* } else { */
	/* 	while (p->next != NULL) { */
	/* 		p = p->next; */
	/* 	} */
	/* 	p->next = info; */
	/* } */

	pthread_mutex_unlock(infoListLock);
}

void *malloc(size_t size)
{
	// add arena info for the current thread to the process info queue
	pthread_once(&once_control, &init);
	if (info == NULL) {
		initArenaInfo();
	}

	// reclaim the resources from other thread if it's a new process
	reclaimResources();

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
	int ret = 0;
	if (hdr == NULL) {
		ret = requestSpaceFromHeap(b);
		if (ret) {
			return NULL;
		}
		hdr = flDequeue(b);
	}
	if (hdr != NULL) {
		ulEnqueue(b, hdr);
		info->sizesOfFL[b]--;
		info->sizesOfUL[b]++;
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
	/* int ret = pthread_mutex_lock(&sbrkLock); */
	/* if (ret == EINVAL) { */
		/* pthread_mutex_init(&sbrkLock, NULL); */
	pthread_mutex_lock(sbrkLock);
	/* } */
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
		flEnqueue(info, b, hdr);
		/* hdr += nodeSize; */
		hdr += nodeSize / 16;
	}
	info->sbrkSpace += requestSize;
	info->sizesOfFL[b] += numOfNewNodes;
	info->totalBlocks[b] += numOfNewNodes;
#if TEST > 0
	printf("%d free slots of %zu BYTE are created!\n", numOfNewNodes, nodeSize);
#endif

	return 0;
}

void free(void *ptr)
{
	// add arena info for the current thread to the process info queue
	pthread_once(&once_control, &init);
	if (info == NULL) {
		initArenaInfo();
	}

	// reclaim the resources from other thread if it's a new process
	reclaimResources();
	
	if (ptr == NULL) return;

	MallocHeader *hdr = ptr - sizeof(MallocHeader);
	
	size_t realSize = hdr->size;
	size_t size = realSize - sizeof(MallocHeader);
	int binInd = sizeToBinNo(size);

	if (size > MAX_SIZE) {
		ulDequeue(-1, hdr);
		munmap((void *)hdr, realSize);
		info->mmapSpace -= realSize;
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

void flEnqueue(ArenaInfo *ai, int qInd, MallocHeader *newHead)
{
	pthread_mutex_lock(&ai->infoLock);
	newHead->next = ai->freeLists[qInd];
	ai->freeLists[qInd] = newHead;
	pthread_mutex_unlock(&ai->infoLock);
}

MallocHeader *flDequeue(int qInd)
{
	pthread_mutex_lock(&info->infoLock);
	MallocHeader *res = info->freeLists[qInd];
	if (res != NULL) {
		info->freeLists[qInd] = res->next;
	}
	pthread_mutex_unlock(&info->infoLock);
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
	/* pthread_mutex_lock(&info->infoLock); */
	MallocHeader *h;
	if (qInd == -1) { // for big space
		h = info->usedListBig;
		if (h == hdrToRemove) {
			info->usedListBig = hdrToRemove->next;
			/* pthread_mutex_unlock(&info->infoLock); */
			return 0;
		}
	} else {
		h = info->usedLists[qInd];
		if (h == hdrToRemove) {
			info->usedLists[qInd] = hdrToRemove->next;
			/* pthread_mutex_unlock(&info->infoLock); */
			return 0;
		}
	}

	while (h != NULL) {
		if (h->next == hdrToRemove) {
			h->next = hdrToRemove->next;
			/* pthread_mutex_unlock(&info->infoLock); */
			return 0;
		}
		h = h->next;
	}
	/* pthread_mutex_unlock(&info->infoLock); */
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
	printf("PID: %d\n", ai->pid);
	printf("TID: %zu\n", ai->tid);
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

int reclaimResources()
{
	// need to acquire the mutex before modify the process's info head
	pthread_mutex_lock(infoListLock);
	pid_t cPid = getpid();
	// TODO check pid/tid of each entry in the list, reclaim resource if possible
	if (infoHead == NULL || cPid == infoHead->pid) {
		pthread_mutex_unlock(infoListLock);
		return 1;
	}

	// the infoLock is sure to be unlock since the infoListLock is unlock
	// so no other thread is access the infoLock of the current thread
	ArenaInfo *p = infoHead;
	/* MallocHeader *hdr; */
	/* MallocHeader *next; */
	while (p != NULL) {
		if (p != info) {
			// reclaim resources from that thread
			reclaimResourcesHelper(p, info);
		}
		p = p->next;
	}
	// set the new head to the ArenaInfo of current thread
	info->pid = cPid;
	info->tid = pthread_self();
	info->next = NULL;
	pthread_mutex_unlock(infoListLock);
	return 0;
}


void reclaimResourcesHelper(ArenaInfo *src, ArenaInfo *dest)
{
	// reclaim memory in BINs
	MallocHeader *hdr;
	MallocHeader *next;
	int i;
	for (i = 0; i < NUM_OF_BINS; i++) {
		hdr = src->freeLists[i];
		while (hdr != NULL) {
			next = hdr->next;
			flEnqueue(dest, i, hdr);
			hdr = next;
		}
		hdr = src->usedLists[i];
		while (hdr != NULL) {
			next = hdr->next;
			flEnqueue(dest, i, hdr);
			hdr = next;
		}
		pthread_mutex_lock(&dest->infoLock);
		dest->sizesOfFL[i] += src->sizesOfFL[i];
		dest->sizesOfFL[i] += src->sizesOfUL[i];
		dest->totalBlocks[i] += src->totalBlocks[i];
		pthread_mutex_unlock(&dest->infoLock);
	}
	// reclaim big memory block
	hdr = src->usedListBig;
	while (hdr != NULL) {
		next = hdr->next;
		munmap((void *)hdr, hdr->size);
		hdr = next;
	}
		
	// summing the total resources allocated
	pthread_mutex_lock(&dest->infoLock);
	dest->sbrkSpace += src->sbrkSpace;
	pthread_mutex_unlock(&dest->infoLock);
}
			
void *realloc(void *ptr, size_t size)
{
	// add arena info for the current thread to the process info queue
	pthread_once(&once_control, &init);
	if (info == NULL) {
		initArenaInfo();
	}

	// reclaim the resources from other thread if it's a new process
	reclaimResources();

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
	if (info == NULL) {
		initArenaInfo();
	}

	// reclaim the resources from other thread if it's a new process
	reclaimResources();

	if (nmemb == 0 || size == 0) {
		return NULL;
	}

	// allocate space for multiple items
	size_t totalSize = size * nmemb;
	void *p = malloc(totalSize);
	memset(p, 0, totalSize);
	return p;
}
