#include <stdio.h>

int sum(int a, int b);

int main(void) {
	int a = 1;
	int b = 2;
	int s = sum(a, b);
	printf("a is %d, b is %d, and the sum is %d\n", a, b, s);
}

int sum(const int a, const int b) {
	int s = a + b;
	b = s;
	return s;
}
