#include "config.h"
#include "math_ops.h"
//#include <stdio.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define DEBUG_PRINT(x) printf("DEBUG: %s = %d\n", #x, x)

#ifdef FEATURE_ENABLED
int feature_function(int x) {
    return x * FACTOR;
}
#else
int feature_function(int x) {
    return x + 1;
}
#endif

int main() {
    int a = 10, b = 20;
    int result = MAX(a, b);
    DEBUG_PRINT(result);

#ifdef CONFIG_VERBOSE
    printf("Verbose mode: a = %d, b = %d\n", a, b);
#endif

    int math_result = add(a, b) + multiply(a, b);
    printf("Math result: %d\n", math_result);

#ifndef FEATURE_ENABLED
    printf("Feature is disabled\n");
#else
    printf("Feature result: %d\n", feature_function(a));
#endif

    return 0;
}