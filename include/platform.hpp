#ifndef MICHAELCC_PLATFORM_HPP
#define MICHAELCC_PLATFORM_HPP

#include <cstddef>

namespace michaelcc {
    // generic platform info struct
    struct platform_info {
        size_t pointer_size;

        size_t char_size = 1;
        size_t short_size;
        size_t int_size;
        size_t long_size;
        size_t long_long_size;

        size_t float_size;
        size_t double_size;

        size_t register_size;
        size_t max_alignment;
        bool optimize_struct_layout = true;
    };
}

#endif
