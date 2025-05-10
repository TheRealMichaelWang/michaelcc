#include "math_ops.h"

typedef enum color {
    RED,
    GREEN,
    BLUE
} Color;

typedef struct item {
    int id;
    Color color;
    int value;
} Item;

Item create_item(int id, Color color, int value) {
    Item item = { id, color, SQUARE(value) };
    return item;
}

int add(int a, int b) {
    return FAST_ADD(a, b);
}

int multiply(int a, int b) {
    return a * b;
}