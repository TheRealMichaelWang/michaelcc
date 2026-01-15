// Test complex expressions and operator precedence

int test_precedence() {
    int a = 2 + 3 * 4;       // Should be 14
    int b = (2 + 3) * 4;     // Should be 20
    int c = 10 - 4 - 2;      // Should be 4 (left-to-right)
    int d = 100 / 10 / 2;    // Should be 5 (left-to-right)
    return a + b + c + d;
}

int test_comparison() {
    int a = 5;
    int b = 10;
    
    int lt = a < b;          // 1
    int gt = a > b;          // 0
    int le = a <= 5;         // 1
    int ge = b >= 10;        // 1
    int eq = a == 5;         // 1
    int ne = a != b;         // 1
    
    return lt + gt + le + ge + eq + ne;
}

int test_logical() {
    int a = 1;
    int b = 0;
    int c = 5;
    
    int and_result = a && c;  // 1
    int or_result = b || a;   // 1
    int not_result = !b;      // 1
    
    int complex = (a && c) || (b && !a);  // 1
    
    return and_result + or_result + not_result + complex;
}

int test_ternary() {
    int a = 5;
    int b = 10;
    
    int max = (a > b) ? a : b;
    int min = (a < b) ? a : b;
    int abs_diff = (a > b) ? (a - b) : (b - a);
    
    // Nested ternary
    int sign = (a > 0) ? 1 : ((a < 0) ? -1 : 0);
    
    return max + min + abs_diff + sign;
}

int test_mixed() {
    int x = 10;
    int y = 20;
    int z = 5;
    
    // Complex mixed expression
    int result = (x + y) * z - (y / x) + (z % 3);
    
    // With comparisons
    int cond = ((x < y) && (z > 0)) ? (x + z) : (y - z);
    
    return result + cond;
}

int main() {
    int p = test_precedence();
    int c = test_comparison();
    int l = test_logical();
    int t = test_ternary();
    int m = test_mixed();
    
    return 0;
}
