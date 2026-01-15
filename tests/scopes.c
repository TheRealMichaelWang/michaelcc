// Test variable scoping and shadowing

int global_var = 100;

int use_global() {
    return global_var;
}

int modify_global(int new_value) {
    int old = global_var;
    global_var = new_value;
    return old;
}

int test_shadowing() {
    int x = 10;
    int result = 0;
    
    {
        int x = 20;
        result = result + x;  // Uses inner x (20)
        
        {
            int x = 30;
            result = result + x;  // Uses innermost x (30)
        }
        
        result = result + x;  // Back to middle x (20)
    }
    
    result = result + x;  // Uses outer x (10)
    
    return result;  // Should be 20 + 30 + 20 + 10 = 80
}

int test_block_scope() {
    int sum = 0;
    
    {
        int a = 1;
        sum = sum + a;
    }
    
    {
        int a = 2;  // Different 'a'
        sum = sum + a;
    }
    
    {
        int a = 3;  // Yet another 'a'
        sum = sum + a;
    }
    
    return sum;  // Should be 6
}

int test_loop_scope() {
    int total = 0;
    int i = 0;
    
    while (i < 5) {
        int squared = i * i;
        total = total + squared;
        i = i + 1;
    }
    
    // 'squared' not accessible here
    // 'i' is still accessible and equals 5
    
    return total + i;
}

int main() {
    int g1 = use_global();
    int old = modify_global(200);
    int g2 = use_global();
    
    int shadow = test_shadowing();
    int block = test_block_scope();
    int loop = test_loop_scope();
    
    return 0;
}
