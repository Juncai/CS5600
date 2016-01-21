#include "restart.h"
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>

char ckpt_image[1000];
static const char CONTEXT_PATH[] = "./context_ckpt";

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: myrestart IMAGE_PATH.\n");
		return -1;
	}

	// 1. move stack to a infrequently used place
	// allocate 0x5300000 - 0x5301000 with mmap (flag MAP_STACK?)
	char *new_ss = (char*)0x530000000000; // start of stack space
	char *new_sp = (char*)0x5300000fffff; // new stack pointer address
	size_t new_s_len = 0x100000;
	mmap(new_ss, new_s_len, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	// 2. declare a global variable, then copy the image file name to it
	strcpy(ckpt_image, argv[1]);

	// 3. move the stack pointer to the newly allocated address in step 1
	asm volatile ("mov %0,%%rsp;" : : "g" (new_sp) : "memory");

	// immediately call a new function to use the newly allocated stack address
	restore_memory();
	return 0;
}


void restore_memory()
{
	// first remove previous stack space
	// a. load /proc/self/maps and find the stack
	// b. use munmap to clear the space
	remove_current_stack();

	// d. copy register from ckpt header to some pre-allocated memory
	ucontext_t context_to_recover;
	get_context_from_header(&context_to_recover);

	// e. use mmap to allocate memory address from section header
	// f. copy memory dump to it
	restore_memory_helper();

	// g. restore old register using setcontext
	setcontext(&context_to_recover);
}

void get_context_from_header(ucontext_t *c)
{
	int fd;
	fd = open(CONTEXT_PATH, O_RDONLY);
	read(fd, c, sizeof(ucontext_t));
	close(fd);
}

void restore_memory_helper()
{
	int fd;
	fd = open(ckpt_image, O_RDONLY);
	Section s;

	while (read(fd, &s, sizeof(Section)) > 0) {
		// first allocate memory
		mmap(s.start, s.len, get_permission(&s), MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

		// load memory dump from ckpt file
		char buf[s.len];
		if(read(fd, buf, s.len) < 0) {
			printf("Failed to read memory dump from ckpt file!\n");
		}
		// copy memory to the allocated space
		memcpy(s.start, buf, s.len);
	}
	close(fd);
}

int get_permission(Section *s)
{
	int perm =  PROT_WRITE;
	if (s->perm[0] == 'r') {
		perm = perm | PROT_READ;
	}
	if (s->perm[2] == 'x') {
		perm = perm | PROT_EXEC;
	}
	return perm;
}

void read_context(int fd, ucontext_t *c)
{
	read(fd, c, sizeof(ucontext_t));
}

void remove_current_stack()
{
	Section ms = get_stack_section();
	munmap(ms.start, ms.len);
}

Section get_stack_section()
{
	char map_path[] = "/proc/self/maps";
	char line_buffer[1000];
	FILE *ptr_file;
	Section ms;

	ptr_file = fopen(map_path, "r");

	while (fgets(line_buffer, 1000, ptr_file) != NULL) {
		// get memory section header
		get_memory_range_and_name(line_buffer, &ms);
		if (strcmp(ms.name, "[stack]") == 0) {
			break;
		}
	}
	fclose(ptr_file);
	return ms;
}

void get_memory_range_and_name(char *line, Section *s) {
	const char deli[2] = " ";
	char *mem_range = strtok(line, deli);
	char *perm = strtok(NULL, deli);
	char *offset = strtok(NULL, deli);
	char *dev = strtok(NULL, deli);
	char *inode = strtok(NULL, deli);
	char *name = strtok(NULL, deli);
	// check permission, if there is no read permission or it's shared memory, pass
	if (perm[0] == '-' || perm[strlen(perm) - 1] == 's') return;
	// check if it's for 'vsyscall', if true pass
	if (strcmp(trim_space(name), "[vsyscall]") == 0) return;
	strcpy(s->perm, perm);
	process_mem_range(mem_range, s);
}

void process_mem_range(char *mr, Section *ms)
{
	const char deli[2] = "-";
	char *start = strtok(mr, deli);
	char *end = strtok(NULL, deli);
	mtcp_readhex(start, &ms->start);
	mtcp_readhex(end, &ms->end);
	ms->len = ms->end - ms->start;
	return;
}

/* Read decimal number, return value and terminating character */
char mtcp_readhex (char *s, char **value)
{
  char c;
  unsigned long int v;
  unsigned long i;
  v = 0;
  for (i = 0; i < strlen(s); i++) {
    c = s[i];
      if ((c >= '0') && (c <= '9')) c -= '0';
    else if ((c >= 'a') && (c <= 'f')) c -= 'a' - 10;
    else if ((c >= 'A') && (c <= 'F')) c -= 'A' - 10;
    else break;
    v = v * 16 + c;
  }
  *value = (char*)v;
  return (c);
}

char *trim_space(char *s)
{
	char *end;
	while(isspace(*s)) s++;
	if (*s == 0) return s;
	end = s + strlen(s) - 1;
	while(end > s && isspace(*end)) end--;
	*(end+1) = 0;

	return s;
}
