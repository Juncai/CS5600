#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
	char *str;
	str = (char *)malloc(15);
	strcpy(str, "asdfgasdfgasdfgasdfgasdfgasdfg");
	printf("%s\n", str);
	free(str);
	return(0);
}
