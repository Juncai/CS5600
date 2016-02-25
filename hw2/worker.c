#include "worker.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>



int main(int argc, char *argv[])
{
	if (argc != 5) {
		printf("Usage: ./worker -x X -n N\n");
		return 0;
	}
	int x = atoi(argv[2]);
	/* x = (int) strtol(argv[2], (char **)NULL, 10) */
	int n = atoi(argv[4]);

	double res;
	res = compute(x, n);
	printf("%d^%d / n: %f\n", x, n, res);

	return 0;
}

double compute(int x, int n)
{
	if (n == 0) return 1;
	int n_f = 1;
	int i;

	for (i = 1; i <= n; i++) {
		n_f *= i;
	}

	double res = pow(x, n) / n_f;

	return res;
}

