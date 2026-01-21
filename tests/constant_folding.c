// Test constant folding optimizations

// Integer arithmetic constant folding
int test_int_arithmetic() {
    int a = 2 + 3;           // Should fold to 5
    int b = 10 - 4;          // Should fold to 6
    int c = 3 * 7;           // Should fold to 21
    int d = 20 / 4;          // Should fold to 5
    int e = 17 % 5;          // Should fold to 2
    
    // Nested constant expressions
    int f = (2 + 3) * 4;     // Should fold to 20
    int g = 100 / (5 * 2);   // Should fold to 10
    int h = 2 + 3 * 4 - 1;   // Should fold to 13
    
    return a + b + c + d + e + f + g + h;
}

// Signed vs unsigned arithmetic
/*int test_signed_unsigned() {
    int a = -10 / 3;         // Signed division: -3
    int b = -10 % 3;         // Signed modulo: -1
    unsigned int c = 10u / 3u;  // Unsigned division: 3
    unsigned int d = 10u % 3u;  // Unsigned modulo: 1
    
    return a + b + c + d;
}*/

// Bitwise constant folding
int test_bitwise() {
    int a = 0xFF & 0x0F;     // Should fold to 0x0F (15)
    int b = 0xF0 | 0x0F;     // Should fold to 0xFF (255)
    int c = 0xFF ^ 0x0F;     // Should fold to 0xF0 (240)
    int d = 1 << 4;          // Should fold to 16
    int e = 32 >> 2;         // Should fold to 8
    int f = ~0;              // Should fold to -1
    
    // Nested bitwise
    int g = (1 << 3) | (1 << 1);  // Should fold to 10
    
    return a + b + c + d + e + g;
}

// Logical constant folding
int test_logical() {
    int a = 1 && 1;          // Should fold to 1
    int b = 1 && 0;          // Should fold to 0
    int c = 0 || 1;          // Should fold to 1
    int d = 0 || 0;          // Should fold to 0
    int e = !0;              // Should fold to 1
    int f = !5;              // Should fold to 0
    
    // Short-circuit constants
    int g = 5 && 3;          // Should fold to 1 (both non-zero)
    int h = 0 || 7;          // Should fold to 1
    
    return a + b + c + d + e + f + g + h;
}

// Comparison constant folding
int test_comparison() {
    int a = 5 == 5;          // Should fold to 1
    int b = 5 == 3;          // Should fold to 0
    int c = 5 != 3;          // Should fold to 1
    int d = 5 != 5;          // Should fold to 0
    int e = 3 < 5;           // Should fold to 1
    int f = 5 < 3;           // Should fold to 0
    int g = 5 > 3;           // Should fold to 1
    int h = 3 > 5;           // Should fold to 0
    int i = 5 <= 5;          // Should fold to 1
    int j = 5 >= 5;          // Should fold to 1
    
    return a + b + c + d + e + f + g + h + i + j;
}

// Signed comparison constant folding
int test_signed_comparison() {
    int a = -5 < 3;          // Should fold to 1
    int b = -5 > -10;        // Should fold to 1
    int c = -1 < 0;          // Should fold to 1
    int d = -1 > -2;         // Should fold to 1
    
    return a + b + c + d;
}

// Float arithmetic constant folding
double test_float_arithmetic() {
    double a = 2.5 + 3.5;    // Should fold to 6.0
    double b = 10.0 - 4.5;   // Should fold to 5.5
    double c = 3.0 * 2.5;    // Should fold to 7.5
    double d = 15.0 / 3.0;   // Should fold to 5.0
    
    // Nested float expressions
    double e = (2.0 + 3.0) * 4.0;  // Should fold to 20.0
    
    return a + b + c + d + e;
}

// Float comparison constant folding
int test_float_comparison() {
    int a = 3.14 < 3.15;     // Should fold to 1
    int b = 2.5 == 2.5;      // Should fold to 1
    int c = 1.0 != 2.0;      // Should fold to 1
    int d = 5.5 > 5.4;       // Should fold to 1
    int e = 3.0 <= 3.0;      // Should fold to 1
    int f = 4.0 >= 3.9;      // Should fold to 1
    
    return a + b + c + d + e + f;
}

// Mixed int/float constant folding
double test_mixed_int_float() {
    double a = 2.5 + 3;      // int promoted to double: 5.5
    double b = 5 - 2.5;      // int promoted to double: 2.5
    double c = 4 * 2.5;      // int promoted to double: 10.0
    double d = 10 / 4.0;     // int promoted to double: 2.5
    
    return a + b + c + d;
}

// Float logical operations
int test_float_logical() {
    int a = (int)1.0 && (int)2.0;      // Should fold to 1
    int b = (int)0.0 || (int)3.5;      // Should fold to 1
    int c = (int)0.0 && (int)5.0;      // Should fold to 0
    int d = (int)0.0 || (int)0.0;      // Should fold to 0
    
    return a + b + c + d;
}

// Unary constant folding
int test_unary() {
    int a = -(-5);           // Should fold to 5
    int b = ~~10;            // Should fold to 10
    int c = !!5;             // Should fold to 1
    int d = !0;              // Should fold to 1
    int e = -10;             // Should fold to -10
    
    return a + b + c + d + e;
}

// Float unary
double test_float_unary() {
    double a = -(-3.5);      // Should fold to 3.5
    double b = -(2.0 + 1.0); // Should fold to -3.0
    
    return a + b;
}

// Complex nested constant expressions
int test_complex_nested() {
    int a = ((2 + 3) * 4 - 5) / 3;      // ((5) * 4 - 5) / 3 = 15 / 3 = 5
    int b = (1 << 4) | (1 << 2) | 1;    // 16 | 4 | 1 = 21
    int c = (10 > 5) + (3 < 7) + (2 == 2);  // 1 + 1 + 1 = 3
    
    return a + b + c;
}

int main() {
    int i1 = test_int_arithmetic();
    //int i2 = test_signed_unsigned();
    int i3 = test_bitwise();
    int i4 = test_logical();
    int i5 = test_comparison();
    int i6 = test_signed_comparison();
    double f1 = test_float_arithmetic();
    int i7 = test_float_comparison();
    double f2 = test_mixed_int_float();
    int i8 = test_float_logical();
    int i9 = test_unary();
    double f3 = test_float_unary();
    int i10 = test_complex_nested();
    
    return 0;
}
