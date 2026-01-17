// Test enum definitions and usage

enum Color {
    RED,
    GREEN,
    BLUE,
    YELLOW,
    BLACK,
    WHITE
};

enum Direction {
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
};

enum Days {
    MONDAY = 1,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY,
    SUNDAY
};

int is_primary_color(enum Color c) {
    if (c == RED || c == GREEN || c == BLUE) {
        return 1;
    }
    return 0;
}

enum Direction turn_right(enum Direction d) {
    return (enum Direction)((d + 1) % 4);
}

enum Direction turn_left(enum Direction d) {
    return (enum Direction)((d + 3) % 4);
}

enum Direction opposite(enum Direction d) {
    return (enum Direction)((d + 2) % 4);
}

int is_weekend(enum Days day) {
    if (day == SATURDAY || day == SUNDAY) {
        return 1;
    }
    return 0;
}

int is_weekday(enum Days day) {
    return !is_weekend(day);
}

int main() {
    enum Color my_color = RED;
    int primary = is_primary_color(my_color);
    
    enum Direction facing = NORTH;
    enum Direction after_right = turn_right(facing);
    enum Direction after_left = turn_left(facing);
    enum Direction back = opposite(facing);
    
    enum Days today = FRIDAY;
    int weekend = is_weekend(today);
    int weekday = is_weekday(today);
    
    return 0;
}
