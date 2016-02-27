#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>

typedef struct MallocHeader
{
	size_t size;
	struct MallocHeader *next;
} MallocHeader;

__thread MallocHeader *free8 = NULL;
__thread MallocHeader *free64 = NULL;
__thread MallocHeader *free512 = NULL;
pthread_mutex_t mutexSbrk; 


void *mymalloc(size_t size);
void myfree(void *ptr);
void enqueue(MallocHeader *newHead, MallocHeader *oldHead);
void *getFree(MallocHeader *freeHead);

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
	void *sPtr = NULL;
	// TODO: Validate size.
	if (size > 512) {
		size_t allocSize = size + sizeof(MallocHeader);
		/* sPtr = mmap(NULL, allocSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0); */
		sPtr = mmap(NULL, allocSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		assert(sPtr != MAP_FAILED);
		MallocHeader *hdr = (MallocHeader*) sPtr;
		hdr->size = size;
		printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
			 __FILE__, __LINE__, size, allocSize, sPtr);

		return sPtr + sizeof(MallocHeader);

	} else {
		size_t realSize = 8;
		if (size <= 8) {
			realSize = 8;
			sPtr = free8;
			if (free8 != NULL) free8 = free8->next;
		} else if (size <= 64) {
			realSize = 64;
			sPtr = free64;
			if (free64 != NULL) free64 = free64->next;
		} else if (size <= 512) {
			realSize = 512;
			sPtr = free512;
			if (free512 != NULL) free512 = free512->next;
		}

		if (sPtr == NULL) {
			// TODO memory request with size equal page size
			size_t pageSize = sysconf(_SC_PAGESIZE);
			size_t allocSize = realSize + sizeof(MallocHeader);

			sPtr = sbrk((intptr_t)allocSize);

			MallocHeader *hdr = (MallocHeader*) sPtr;
			hdr->size = realSize;
		}
		printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
			 __FILE__, __LINE__, size, realSize + sizeof(MallocHeader), sPtr);

		return sPtr + sizeof(MallocHeader);
	}
}

void myfree(void *ptr)
{
	MallocHeader *hdr = ptr - sizeof(MallocHeader);
	size_t realSize = hdr->size + sizeof(MallocHeader);
	// TODO add the address to the free list of the thread
	if (hdr->size == 8) {
		hdr->next = free8;
		free8 = hdr;
	} else if (hdr->size == 64) {
		hdr->next = free8;
		free8 = hdr;
	} else if (hdr->size == 512) {
		hdr->next = free8;
		free8 = hdr;
	} else if (hdr->size > 512) {
		munmap((void *)hdr, realSize);
	}

	printf("%s:%d free(%p): Freeing %zu bytes from %p\n",
		 __FILE__, __LINE__, ptr, realSize, hdr);
}

/* void *getFree(MallocHeader *freeHead) */
/* { */
/* 	MallocHeader *res = freeHead; */
/* 	if (freeHead != NULL) { */
/* 		freeHead = freeHead->next; */
/* 	} */
/* 	return res; */
/* } */

/* void enqueue(MallocHeader *newHead, MallocHeader *oldHead) */
/* { */
/* 	newHead->next = oldHead; */
/* 	oldHead = newHead; */
/* } */

/* void *realloc(void *ptr, size_t size) */
/* { */
/*   // Allocate new memory (if needed) and copy the bits from old location to new. */

/*   return NULL; */
/* } */

