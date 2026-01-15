// Test forward declarations

// Forward declarations
int helper_function(int x);
int another_helper(int a, int b);
int recursive_helper(int n);

// Functions that use forward-declared functions
int main_logic(int input) {
    int step1 = helper_function(input);
    int step2 = another_helper(step1, input);
    int step3 = recursive_helper(step2 % 10);
    return step3;
}

// Mutually recursive functions (forward declared)
int is_even(int n);
int is_odd(int n);

int is_even(int n) {
    if (n == 0) {
        return 1;
    }
    return is_odd(n - 1);
}

int is_odd(int n) {
    if (n == 0) {
        return 0;
    }
    return is_even(n - 1);
}

// Implementations of forward-declared functions
int helper_function(int x) {
    return x * 2 + 1;
}

int another_helper(int a, int b) {
    return a + b - 5;
}

int recursive_helper(int n) {
    if (n <= 1) {
        return n;
    }
    return recursive_helper(n - 1) + recursive_helper(n - 2);
}

int main() {
    int result = main_logic(42);
    
    int even_check = is_even(10);
    int odd_check = is_odd(7);
    
    return 0;
}
