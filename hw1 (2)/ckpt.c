#include "ckpt.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>

static const char CONTEXT_PATH[] = "./context_ckpt";
static const char IMG_PATH[] = "./myckpt";
static const char MEM_MAP[] = "/proc/self/maps";
int from_recover = 1;

__attribute__((constructor))
void myconstructor() {
	signal(SIGUSR2, signal_handler);
}

void signal_handler(int signo) {
	if (signo == SIGUSR2) {
		dump_img();		
	}
}

void dump_img(void) {
	// 2. read /proc/self/maps to get section headers, then get memory dump
	create_memory_checkpoint();

	// 1. getcontext to save registers values
	ucontext_t mycontext;
	from_recover = 0;
    getcontext(&mycontext);
	// if this is not from recover, save the context and exit the program
	if (from_recover == 0) {
		write_context_to_ckpt_header(&mycontext, sizeof mycontext);
		exit(0);
	}
}

void create_memory_checkpoint()
{
	Section ms;
	char line_buffer[1000];
	FILE *ptr_file;

	ptr_file = fopen(MEM_MAP, "r");

	while (fgets(line_buffer, 1000, ptr_file) != NULL) {
		// get memory section header
		get_memory_range_and_permission(line_buffer, &ms);
		// write memory section header and memory dump to the ckpt file
		write_memory_section_to_ckpt(&ms);
	}
	fclose(ptr_file);
}

void write_memory_section_to_ckpt(Section *ms)
{
	int fd;
	fd = open(IMG_PATH, O_RDWR | O_APPEND | O_CREAT, 0666);
	if (fd < 0) {
		printf("Failed to open myckpt file!\n");
	}
	if (write(fd, ms, sizeof(Section)) < 0) {
		printf("Failed to write section header to myckpt!\n");
	}
	if (write(fd, ms->start, ms->len) < 0) {
		printf("Failed to write memory dump to myckpt!\n");
	}	
	close(fd);
}

void get_memory_range_and_permission(char *line, Section *s) {
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

void fwrite_to_ckpt (void *p, size_t size, char *fpath)
{
	FILE *file = fopen(fpath, "wb");
	if (file != NULL) {
		fwrite(p, size, 1, file);
		fclose(file);
	}
}

void process_mem_range(char *mr, Section *ms)
{
	const char deli[2] = "-";
	char *start = strtok(mr, deli);
	char *end = strtok(NULL, deli);
	mtcp_readhex(start, &ms->start);
	mtcp_readhex(end, &ms->end);
	ms->len = ms->end - ms->start;
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

void write_to_ckpt(const void *buffer, int context_len) {
	int fd;
	fd = open(IMG_PATH, O_RDWR | O_APPEND | O_CREAT, 0666);
	if (fd < 0) {
		printf("Failed to open myckpt file!\n");
	}
	if (write(fd, buffer, context_len) < 0) {
		printf("Failed to write to myckpt!\n");
	}
}

void write_context_to_ckpt_header(ucontext_t *context, int context_len) {
	int fd;
	// this is the first write to the file, if the file exists, overwrite it!
	fd = open(CONTEXT_PATH, O_RDWR | O_TRUNC | O_CREAT, 0666);
	if (fd < 0) {
		printf("Failed to open myckpt file!\n");
	}
	if (write(fd, context, context_len) < 0) {
		printf("Failed to write to myckpt!\n");
	}
	close(fd);
}

int save_ckpt_img(void) {
	return 0;	
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
