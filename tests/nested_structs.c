// Test nested structures and complex data types

struct Vector2D {
    int x;
    int y;
};

struct Vector3D {
    int x;
    int y;
    int z;
};

struct Transform {
    struct Vector3D position;
    struct Vector3D rotation;
    struct Vector3D scale;
};

struct AABB {
    struct Vector3D min;
    struct Vector3D max;
};

int dot_product_2d(struct Vector2D a, struct Vector2D b) {
    return a.x * b.x + a.y * b.y;
}

int dot_product_3d(struct Vector3D a, struct Vector3D b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

int magnitude_squared_2d(struct Vector2D v) {
    return v.x * v.x + v.y * v.y;
}

int magnitude_squared_3d(struct Vector3D v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

struct Vector3D add_vectors(struct Vector3D a, struct Vector3D b) {
    struct Vector3D result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

struct Vector3D scale_vector(struct Vector3D v, int s) {
    struct Vector3D result;
    result.x = v.x * s;
    result.y = v.y * s;
    result.z = v.z * s;
    return result;
}

int aabb_volume(struct AABB box) {
    int width = box.max.x - box.min.x;
    int height = box.max.y - box.min.y;
    int depth = box.max.z - box.min.z;
    return width * height * depth;
}

void translate_transform(struct Transform* t, struct Vector3D offset) {
    t->position = add_vectors(t->position, offset);
}

int main() {
    struct Vector2D v2a;
    v2a.x = 3;
    v2a.y = 4;
    
    struct Vector2D v2b;
    v2b.x = 1;
    v2b.y = 2;
    
    int dot2 = dot_product_2d(v2a, v2b);
    int mag2 = magnitude_squared_2d(v2a);
    
    struct Vector3D v3a;
    v3a.x = 1;
    v3a.y = 2;
    v3a.z = 3;
    
    struct Vector3D v3b;
    v3b.x = 4;
    v3b.y = 5;
    v3b.z = 6;
    
    int dot3 = dot_product_3d(v3a, v3b);
    struct Vector3D sum = add_vectors(v3a, v3b);
    struct Vector3D scaled = scale_vector(v3a, 2);
    
    struct Transform transform;
    transform.position.x = 0;
    transform.position.y = 0;
    transform.position.z = 0;
    transform.rotation.x = 0;
    transform.rotation.y = 0;
    transform.rotation.z = 0;
    transform.scale.x = 1;
    transform.scale.y = 1;
    transform.scale.z = 1;
    
    translate_transform(&transform, v3a);
    
    struct AABB box;
    box.min.x = 0;
    box.min.y = 0;
    box.min.z = 0;
    box.max.x = 10;
    box.max.y = 20;
    box.max.z = 30;
    
    int volume = aabb_volume(box);
    
    return 0;
}
