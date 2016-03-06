#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


int main() {
	long pageSize = sysconf(_SC_PAGESIZE);
	printf("Pagesize: %ld\n", pageSize);

	char *str = (char *)malloc(8);
	strcpy(str, "haha");
	printf("%s\n", str);
	char *str1 = (char *)malloc(32);
	char *str2 = (char *)malloc(32);
	char *str3 = (char *)malloc(32);

	free(str);
	free(str3);
	free(str1);
	free(str2);

	// should reuse the previously free slot
	/* str = (char *)mymalloc(8); */
	/* strcpy(str, "haha"); */
	/* printf("%s\n", str); */
	/* myfree(str); */

	return(0);
}
