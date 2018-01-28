#include <stdio.h>

void main() {
	int a, b;
	a = 5;
	b = 3;

	while (a > b) {
		a = a - b;
	}
	printf("%d \n", a);
}
