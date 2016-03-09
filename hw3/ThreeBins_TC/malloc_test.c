#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>

int main()
{
	char *str;
	str = (char *)malloc(15);
	strcpy(str, "dfgasdfgasdfgasdfg");
	int realSize = 48;
	int MIN_SIZE = 32;
	double d = realSize / MIN_SIZE;
	printf("d = %f\n", d);
	int flIndex = (int)ceil(log(d) / log(2));
	printf("index = %d\n", flIndex);
	printf("%s\n", str);
	malloc_stats();
	free(str);
	int *aa[2];
	int i1 = 1;
	int i2 = 2;
	int i3 = 3;
	aa[0] = &i1;
	int *ii;
	ii = &i3;

	aa[1] = aa[0];
	aa[0] = ii;
	printf("aa0: %d, aa1: %d\n", *aa[0], *aa[1]);
	return(0);
}
