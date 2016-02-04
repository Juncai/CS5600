#include <stdio.h>
#include <unistd.h>

void print_dots();

int main(void) {
	print_dots();
	return 0;
}

void print_dots()
{
	while (1) {
		printf("%c", '.');
		fflush(stdout);
		sleep(1);
	}
}
