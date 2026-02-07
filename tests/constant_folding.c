// Test constant folding optimizations

// const integer arithmetic constant folding
inline const int test_int_arithmetic() {
    const int a = 2 + 3;           // Should fold to 5
    const int b = 10 - 4;          // Should fold to 6
    const int c = 3 * 7;           // Should fold to 21
    const int d = 20 / 4;          // Should fold to 5
    const int e = 17 % 5;          // Should fold to 2
    
    // Nested constant expressions
    const int f = (2 + 3) * 4;     // Should fold to 20
    const int g = 100 / (5 * 2);   // Should fold to 10
    const int h = 2 + 3 * 4 - 1;   // Should fold to 13
    
    return a + b + c + d + e + f + g + h;
}

// Signed vs unsigned arithmetic
/*const int test_signed_unsigned() {
    const int a = -10 / 3;         // Signed division: -3
    const int b = -10 % 3;         // Signed modulo: -1
    unsigned const int c = 10u / 3u;  // Unsigned division: 3
    unsigned const int d = 10u % 3u;  // Unsigned modulo: 1
    
    return a + b + c + d;
}*/

// Bitwise constant folding
inline const int test_bitwise() {
    const int a = 0xFF & 0x0F;     // Should fold to 0x0F (15)
    const int b = 0xF0 | 0x0F;     // Should fold to 0xFF (255)
    const int c = 0xFF ^ 0x0F;     // Should fold to 0xF0 (240)
    const int d = 1 << 4;          // Should fold to 16
    const int e = 32 >> 2;         // Should fold to 8
    const int f = ~0;              // Should fold to -1
    
    // Nested bitwise
    const int g = (1 << 3) | (1 << 1);  // Should fold to 10
    
    return a + b + c + d + e + g;
}

// Logical constant folding
inline const int test_logical() {
    const int a = 1 && 1;          // Should fold to 1
    const int b = 1 && 0;          // Should fold to 0
    const int c = 0 || 1;          // Should fold to 1
    const int d = 0 || 0;          // Should fold to 0
    const int e = !0;              // Should fold to 1
    const int f = !5;              // Should fold to 0
    
    // Short-circuit constants
    const int g = 5 && 3;          // Should fold to 1 (both non-zero)
    const int h = 0 || 7;          // Should fold to 1
    
    return a + b + c + d + e + f + g + h;
}

// Comparison constant folding
inline const int test_comparison() {
    const int a = 5 == 5;          // Should fold to 1
    const int b = 5 == 3;          // Should fold to 0
    const int c = 5 != 3;          // Should fold to 1
    const int d = 5 != 5;          // Should fold to 0
    const int e = 3 < 5;           // Should fold to 1
    const int f = 5 < 3;           // Should fold to 0
    const int g = 5 > 3;           // Should fold to 1
    const int h = 3 > 5;           // Should fold to 0
    const int i = 5 <= 5;          // Should fold to 1
    const int j = 5 >= 5;          // Should fold to 1
    
    return a + b + c + d + e + f + g + h + i + j;
}

// Signed comparison constant folding
inline const int test_signed_comparison() {
    const int a = -5 < 3;          // Should fold to 1
    const int b = -5 > -10;        // Should fold to 1
    const int c = -1 < 0;          // Should fold to 1
    const int d = -1 > -2;         // Should fold to 1
    
    return a + b + c + d;
}

// Float arithmetic constant folding
inline const double test_float_arithmetic() {
    const double a = 2.5 + 3.5;    // Should fold to 6.0
    const double b = 10.0 - 4.5;   // Should fold to 5.5
    const double c = 3.0 * 2.5;    // Should fold to 7.5
    const double d = 15.0 / 3.0;   // Should fold to 5.0
    
    // Nested float expressions
    const double e = (2.0 + 3.0) * 4.0;  // Should fold to 20.0
    
    return a + b + c + d + e;
}

// Float comparison constant folding
inline const int test_float_comparison() {
    const int a = 3.14 < 3.15;     // Should fold to 1
    const int b = 2.5 == 2.5;      // Should fold to 1
    const int c = 1.0 != 2.0;      // Should fold to 1
    const int d = 5.5 > 5.4;       // Should fold to 1
    const int e = 3.0 <= 3.0;      // Should fold to 1
    const int f = 4.0 >= 3.9;      // Should fold to 1
    
    return a + b + c + d + e + f;
}

// Mixed const int/float constant folding
inline const double test_mixed_int_float() {
    const double a = 2.5 + 3;      // const int promoted to double: 5.5
    const double b = 5 - 2.5;      // const int promoted to double: 2.5
    const double c = 4 * 2.5;      // const int promoted to double: 10.0
    const double d = 10 / 4.0;     // const int promoted to double: 2.5
    
    return a + b + c + d;
}

// Float logical operations
inline const int test_float_logical() {
    const int a = (const int)1.0 && (const int)2.0;      // Should fold to 1
    const int b = (const int)0.0 || (const int)3.5;      // Should fold to 1
    const int c = (const int)0.0 && (const int)5.0;      // Should fold to 0
    const int d = (const int)0.0 || (const int)0.0;      // Should fold to 0
    
    return a + b + c + d;
}

// Unary constant folding
inline const int test_unary() {
    const int a = -(-5);           // Should fold to 5
    const int b = ~~10;            // Should fold to 10
    const int c = !!5;             // Should fold to 1
    const int d = !0;              // Should fold to 1
    const int e = -10;             // Should fold to -10
    
    return a + b + c + d + e;
}

// Float unary
inline const double test_float_unary() {
    const double a = -(-3.5);      // Should fold to 3.5
    const double b = -(2.0 + 1.0); // Should fold to -3.0
    
    return a + b;
}

// Complex nested constant expressions
inline const int test_complex_nested() {
    const int a = ((2 + 3) * 4 - 5) / 3;      // ((5) * 4 - 5) / 3 = 15 / 3 = 5
    const int b = (1 << 4) | (1 << 2) | 1;    // 16 | 4 | 1 = 21
    const int c = (10 > 5) + (3 < 7) + (2 == 2);  // 1 + 1 + 1 = 3
    
    return a + b + c;
}

const int main() {
    int i1 = test_int_arithmetic();
    //const int i2 = test_signed_unsigned();
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
