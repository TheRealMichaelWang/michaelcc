// Test basic arithmetic operations

int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int multiply(int a, int b) {
    return a * b;
}

int divide(int a, int b) {
    return a / b;
}

int modulo(int a, int b) {
    return a % b;
}

int main() {
    int x = 10;
    int y = 3;
    
    int sum = add(x, y);
    int diff = subtract(x, y);
    int prod = multiply(x, y);
    int quot = divide(x, y);
    int rem = modulo(x, y);
    
    // Compound expressions
    int result = (x + y) * 2 - 1;
    int complex = add(multiply(x, 2), subtract(y, 1));
    
    return 0;
}
