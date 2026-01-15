// Test loop constructs

int sum_to_n(int n) {
    int sum = 0;
    int i = 1;
    while (i <= n) {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}

int factorial(int n) {
    int result = 1;
    int i = 2;
    while (i <= n) {
        result = result * i;
        i = i + 1;
    }
    return result;
}

int count_digits(int n) {
    if (n == 0) {
        return 1;
    }
    if (n < 0) {
        n = -n;
    }
    int count = 0;
    while (n > 0) {
        count = count + 1;
        n = n / 10;
    }
    return count;
}

int power(int base, int exp) {
    int result = 1;
    while (exp > 0) {
        result = result * base;
        exp = exp - 1;
    }
    return result;
}

int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int main() {
    int s = sum_to_n(10);
    int f = factorial(5);
    int d = count_digits(12345);
    int p = power(2, 8);
    int g = gcd(48, 18);
    
    return 0;
}
