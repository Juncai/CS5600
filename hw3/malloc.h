#include <stdio.h>

#define MIN_SIZE 32
#define MAX_SIZE 4096
#define PAGES_REQUESTED 4
#define NUM_OF_BINS 8

typedef struct MallocHeader
{
	size_t size;
	struct MallocHeader *next;
} MallocHeader;

void *malloc(size_t size);
void free(void *ptr);
