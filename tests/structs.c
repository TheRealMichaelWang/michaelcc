// Test struct definitions and usage

struct Point {
    int x;
    int y;
};

struct Rectangle {
    struct Point top_left;
    struct Point bottom_right;
};

void make_point(struct Point* p, int x, int y) {
    p->x = x;
    p->y = y;
}

int distance_squared(struct Point* a, struct Point* b) {
    int dx = b->x - a->x;
    int dy = b->y - a->y;
    return dx * dx + dy * dy;
}

int rect_width(struct Rectangle* r) {
    return r->bottom_right.x - r->top_left.x;
}

int rect_height(struct Rectangle* r) {
    return r->bottom_right.y - r->top_left.y;
}

int rect_area(struct Rectangle* r) {
    return rect_width(r) * rect_height(r);
}

void move_point(struct Point* p, int dx, int dy) {
    p->x = p->x + dx;
    p->y = p->y + dy;
}

int main() {
    struct Point origin;
    origin.x = 0;
    origin.y = 0;
    
    struct Point p1;
    make_point(&p1, 3, 4);
    struct Point p2;
    make_point(&p2, 6, 8);
    
    int dist_sq = distance_squared(&origin, &p1);
    
    struct Rectangle rect;
    rect.top_left = origin;
    rect.bottom_right = p2;
    
    int area = rect_area(&rect);
    int width = rect_width(&rect);
    int height = rect_height(&rect);
    
    move_point(&p1, 1, 1);
    
    return 0;
}
