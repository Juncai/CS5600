#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <ucontext.h>
#include <regex.h>

#define BUF_SIZE 2048
	
int save_ckpt_img(void);
// char mtcp_readhex (int fd, VA *value);
void signal_handler(int signo); 
void dump_img(void);
void write_context_to_ckpt_header(ucontext_t *context, int context_len);
int get_memory_range_and_permission(char *line, char *res);
void write_to_ckpt(const void *buffer, int context_len);

__attribute__((constructor))
void myconstructor() {
	signal(SIGUSR2, signal_handler);
}

int main(void) {
	int total_num = 100;
	int i;
	for (i = 0; i < total_num; i++) {
		printf("%d\n", i);
		sleep(1);
	}
	return 0;
}

void signal_handler(int signo) {
	if (signo == SIGUSR2) {
		/* printf("receive SIGUSR2! Start dumping image!\n"); */
		dump_img();		
		exit(0);
	}
}

void dump_img(void) {
	/* int fd_maps; */
	char map_path[] = "/proc/self/maps";
	char line_buffer[1000];
	FILE *ptr_file;

	/* fd_maps = open(map_path, O_RDONLY); */
	/* if (fd_maps < 0) { */
	/* 	printf("Failed to open memory maps!\n"); */
	/* 	exit(-1); */
	/* } */
	// ckpt header: ucontext_t
	// section header: memory range + length + permission
	// section image: memory dump
	// 1. getcontext to save registers values
	ucontext_t mycontext;
    if (getcontext(&mycontext) < 0) {
		printf("Failed to get context!\n");
	}
	write_context_to_ckpt_header(&mycontext, sizeof mycontext);
	
	// 2. read /proc/self/maps to get section headers, then get memory dump
	ptr_file = fopen(map_path, "r");

	char resbuffer[256];
	while (fgets(line_buffer, 1000, ptr_file) != NULL) {
		printf("%s", line_buffer);
		if (get_memory_range_and_permission(line_buffer, resbuffer) >= 0) {
			write_to_ckpt(&resbuffer, sizeof resbuffer);
		}
			
	}
	fclose(ptr_file);


}

int get_memory_range_and_permission(char *line, char *res) {
	regex_t regex;
	int reti;

	return 0;
}

void write_to_ckpt(const void *buffer, int context_len) {
	char imgpath[] = "./myckpt";
	int fd;
	fd = open(imgpath, O_RDWR | O_APPEND | O_CREAT, 0666);
	if (fd < 0) {
		printf("Failed to open myckpt file!\n");
	}
	if (write(fd, buffer, context_len) < 0) {
		printf("Failed to write to myckpt!\n");
	}
}



void write_context_to_ckpt_header(ucontext_t *context, int context_len) {
	char imgpath[] = "./myckpt";
	int fd;
	fd = open(imgpath, O_RDWR | O_APPEND | O_CREAT, 0666);
	if (fd < 0) {
		printf("Failed to open myckpt file!\n");
	}
	if (write(fd, context, context_len) < 0) {
		printf("Failed to write to myckpt!\n");
	}
}


int code_pool(void) {
	char buffer[BUF_SIZE];
	char ckptFile[] = "./myckpt";
	int fd;
	int read_in, write_out;
	char someContent[] = "hahahaha, hello world!";

    printf("Hello world!\n");
    
	fd = creat(ckptFile, 0666);

	if (fd < 0) {
        return -1;
	}

	write_out = write(fd, someContent, 10);
	close (fd);

	fd = open(ckptFile, O_RDWR);
	read_in = read(fd, &buffer, BUF_SIZE);
	printf("Read in: %d, Content of the file: %s\n", read_in, buffer);

	close (fd);
	return 0;
}

int save_ckpt_img(void) {
	return 0;	
}


/* Read decimal number, return value and terminating character */
char mtcp_readhex (int fd, VA *value)
{
  char c;
  unsigned long int v;

  v = 0;
  while (1) {
    c = mtcp_readchar (fd);
      if ((c >= '0') && (c <= '9')) c -= '0';
    else if ((c >= 'a') && (c <= 'f')) c -= 'a' - 10;
    else if ((c >= 'A') && (c <= 'F')) c -= 'A' - 10;
    else break;
    v = v * 16 + c;
  }
  *value = (VA)v;
  return (c);
}
