#ifndef MICHAELCC_PLATFORM_HPP
#define MICHAELCC_PLATFORM_HPP

#include "linear/registers.hpp"
#include <cstddef>
#include <linear/registers.hpp>
#include <vector>

namespace michaelcc {
    // generic platform info struct
    struct platform_info {
        linear::word_size pointer_size;

        linear::word_size char_size = linear::word_size::MICHAELCC_WORD_SIZE_BYTE;
        linear::word_size short_size;
        linear::word_size int_size;
        linear::word_size long_size;
        linear::word_size long_long_size;

        linear::word_size float_size;
        linear::word_size double_size;

        size_t max_alignment;
        bool optimize_struct_layout = true;

        uint8_t return_value_register_id;

        std::vector<linear::register_info> registers;

        const linear::register_info& get_register_info(uint8_t id) const {
            return registers.at(id);
        }

        size_t max_register_size() const {
            return std::max_element(registers.begin(), registers.end(), [](const linear::register_info& a, const linear::register_info& b) {
                return a.size_bits < b.size_bits;
            })->size_bits;
        }
    };
}

#endif
