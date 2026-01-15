// Test bitwise operations

int bit_and(int a, int b) {
    return a & b;
}

int bit_or(int a, int b) {
    return a | b;
}

int bit_xor(int a, int b) {
    return a ^ b;
}

int bit_not(int a) {
    return ~a;
}

int left_shift(int a, int n) {
    return a << n;
}

int right_shift(int a, int n) {
    return a >> n;
}

int get_bit(int value, int bit) {
    return (value >> bit) & 1;
}

int set_bit(int value, int bit) {
    return value | (1 << bit);
}

int clear_bit(int value, int bit) {
    return value & ~(1 << bit);
}

int toggle_bit(int value, int bit) {
    return value ^ (1 << bit);
}

int count_bits(int value) {
    int count = 0;
    while (value != 0) {
        count = count + (value & 1);
        value = value >> 1;
    }
    return count;
}

int is_power_of_two(int n) {
    if (n <= 0) {
        return 0;
    }
    return (n & (n - 1)) == 0;
}

int main() {
    int a = 0b10101010;
    int b = 0b11001100;
    
    int and_result = bit_and(a, b);
    int or_result = bit_or(a, b);
    int xor_result = bit_xor(a, b);
    int not_result = bit_not(a);
    
    int shifted_left = left_shift(a, 2);
    int shifted_right = right_shift(a, 2);
    
    int bit3 = get_bit(a, 3);
    int with_bit_set = set_bit(0, 5);
    int with_bit_cleared = clear_bit(a, 1);
    int with_bit_toggled = toggle_bit(a, 0);
    
    int bits_in_a = count_bits(a);
    int is_pow2 = is_power_of_two(16);
    
    return 0;
}
