// Test recursive functions

int factorial_rec(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial_rec(n - 1);
}

int fibonacci(int n) {
    if (n <= 0) {
        return 0;
    }
    if (n == 1) {
        return 1;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int sum_rec(int n) {
    if (n <= 0) {
        return 0;
    }
    return n + sum_rec(n - 1);
}

int power_rec(int base, int exp) {
    if (exp == 0) {
        return 1;
    }
    return base * power_rec(base, exp - 1);
}

int gcd_rec(int a, int b) {
    if (b == 0) {
        return a;
    }
    return gcd_rec(b, a % b);
}

int ackermann(int m, int n) {
    if (m == 0) {
        return n + 1;
    }
    if (n == 0) {
        return ackermann(m - 1, 1);
    }
    return ackermann(m - 1, ackermann(m, n - 1));
}

int main() {
    int f = factorial_rec(6);
    int fib = fibonacci(10);
    int s = sum_rec(100);
    int p = power_rec(3, 4);
    int g = gcd_rec(252, 105);
    int a = ackermann(2, 3);
    
    return 0;
}
