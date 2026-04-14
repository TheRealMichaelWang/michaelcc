// Test union definitions and usage

union IntOrFloat {
    int as_int;
    float as_float;
};

union Value {
    int integer;
    char byte;
    short word;
};

struct TaggedValue {
    int tag;  // 0 = int, 1 = short, 2 = byte
    union Value value;
};

int get_as_int(union Value v) {
    return v.integer;
}

void set_int(union Value* v, int value) {
    v->integer = value;
}

void set_byte(union Value* v, char value) {
    v->byte = value;
}

int extract_tagged_int(struct TaggedValue tv) {
    if (tv.tag == 0) {
        return tv.value.integer;
    } else if (tv.tag == 1) {
        return (int)tv.value.word;
    } else {
        return (int)tv.value.byte;
    }
}

int main() {
    union IntOrFloat iof = { 42 };
    int i = iof.as_int;
    
    union Value v = { 0x12345678 };
    
    int full = v.integer;
    char low_byte = v.byte;
    short low_word = v.word;
    
    set_int(&v, 100);
    int after_set = v.integer;
    
    struct TaggedValue tv = { 0, { 999 } };
    int extracted = extract_tagged_int(tv);
    
    return 0;
}
