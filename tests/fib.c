#include "include.h"
#include "include.h"

#ifdef MICHAEL
int michael = 200;
#else
#ifdef GUARD_H
int guard_defined = 100;
#endif // GUARD_H
#endif

#define max(a, b) ((a) > (b) ? (a) : (b))

int l = max(2, 5);

int fib(int n) {
	if (n <= 1) {
		return n;
	}
	return fib(n - 1) + fib(n - 2);
}