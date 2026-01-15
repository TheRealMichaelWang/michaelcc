// Test control flow statements

int abs_value(int x) {
    if (x < 0) {
        return -x;
    } else {
        return x;
    }
}

int max(int a, int b) {
    if (a > b) {
        return a;
    }
    return b;
}

int min(int a, int b) {
    return (a < b) ? a : b;
}

int clamp(int value, int low, int high) {
    if (value < low) {
        return low;
    } else if (value > high) {
        return high;
    } else {
        return value;
    }
}

int sign(int x) {
    if (x > 0) {
        return 1;
    } else if (x < 0) {
        return -1;
    }
    return 0;
}

int main() {
    int a = -5;
    int b = 10;
    
    int abs_a = abs_value(a);
    int maximum = max(a, b);
    int minimum = min(a, b);
    int clamped = clamp(15, 0, 10);
    int s = sign(a);
    
    return 0;
}
