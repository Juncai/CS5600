#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUF_SIZE 2048

int main(void) {
	char buffer[BUF_SIZE];
	char fileName[] = "./test_file";
	int fd;
	int read_in, write_out;
	char someContent[] = "hahahaha, hello world!";

    printf("Hello world!\n");
    
	fd = creat(fileName, 0666);

	if (fd < 0) {
        return -1;
	}

	write_out = write(fd, someContent, 10);
	close (fd);

	fd = open(fileName, O_RDWR);
	read_in = read(fd, &buffer, BUF_SIZE);
	printf("Read in: %d, Content of the file: %s\n", read_in, buffer);

	close (fd);
}
 
