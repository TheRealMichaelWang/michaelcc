// Test array operations

int sum_array(int* arr, int size) {
    int sum = 0;
    int i = 0;
    while (i < size) {
        sum = sum + arr[i];
        i = i + 1;
    }
    return sum;
}

int find_max(int* arr, int size) {
    int max = arr[0];
    int i = 1;
    while (i < size) {
        if (arr[i] > max) {
            max = arr[i];
        }
        i = i + 1;
    }
    return max;
}

int find_min(int* arr, int size) {
    int min = arr[0];
    int i = 1;
    while (i < size) {
        if (arr[i] < min) {
            min = arr[i];
        }
        i = i + 1;
    }
    return min;
}

void reverse_array(int* arr, int size) {
    int i = 0;
    int j = size - 1;
    while (i < j) {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
        i = i + 1;
        j = j - 1;
    }
}

int count_value(int* arr, int size, int value) {
    int count = 0;
    int i = 0;
    while (i < size) {
        if (arr[i] == value) {
            count = count + 1;
        }
        i = i + 1;
    }
    return count;
}

int main() {
    int numbers[10];
    numbers[0] = 5;
    numbers[1] = 2;
    numbers[2] = 8;
    numbers[3] = 1;
    numbers[4] = 9;
    numbers[5] = 3;
    numbers[6] = 7;
    numbers[7] = 4;
    numbers[8] = 6;
    numbers[9] = 0;
    
    int total = sum_array(numbers, 10);
    int maximum = find_max(numbers, 10);
    int minimum = find_min(numbers, 10);
    
    reverse_array(numbers, 10);
    
    int zeros = count_value(numbers, 10, 0);
    
    return 0;
}
