#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

typedef struct MallocHeader
{
  size_t size;
} MallocHeader;


void *mymalloc(size_t size);
void myfree(void *ptr);

int main() {
	char *str;
	str = (char *)mymalloc(7);
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
  // TODO: Validate size.
  size_t allocSize = size + sizeof(MallocHeader);

  /* void *ret = mmap(0, allocSize, PROT_READ | PROT_WRITE, */
  /*                  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0); */
  /* assert(ret != MAP_FAILED); */

  printf("%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
         __FILE__, __LINE__, size, allocSize, ret);

  MallocHeader *hdr = (MallocHeader*) ret;
  hdr->size = allocSize;

  return ret + sizeof(MallocHeader);
}

void myfree(void *ptr)
{
  MallocHeader *hdr = ptr - sizeof(MallocHeader);
  printf("%s:%d free(%p): Freeing %zu bytes from %p\n",
         __FILE__, __LINE__, ptr, hdr->size, hdr);
  munmap(hdr, hdr->size);
}

/* void *realloc(void *ptr, size_t size) */
/* { */
/*   // Allocate new memory (if needed) and copy the bits from old location to new. */

/*   return NULL; */
/* } */

