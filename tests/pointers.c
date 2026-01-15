// Test pointer operations

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void increment(int* value) {
    *value = *value + 1;
}

int dereference_and_add(int* a, int* b) {
    return *a + *b;
}

void set_to_zero(int* ptr) {
    *ptr = 0;
}

int* max_ptr(int* a, int* b) {
    if (*a > *b) {
        return a;
    }
    return b;
}

void add_to_value(int* ptr, int amount) {
    *ptr = *ptr + amount;
}

int main() {
    int x = 5;
    int y = 10;
    
    swap(&x, &y);
    
    int* px = &x;
    int* py = &y;
    
    increment(px);
    
    int sum = dereference_and_add(px, py);
    
    int* larger = max_ptr(&x, &y);
    int max_val = *larger;
    
    set_to_zero(&x);
    
    return 0;
}
