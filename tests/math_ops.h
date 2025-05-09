#ifndef MATH_OPS_H
#define MATH_OPS_H

#define FACTOR 5
#define SQUARE(x) ((x) * (x))

int add(int a, int b);
int multiply(int a, int b);

#ifdef USE_FAST_MATH
#define FAST_ADD(x, y) ((x) + (y) + 1) // Simplified for testing
#else
#define FAST_ADD(x, y) ((x) + (y))
#endif

#endif // MATH_OPS_H