#ifndef MICHAELCC_PLATFORM_HPP
#define MICHAELCC_PLATFORM_HPP

#include <cstddef>
#include <linear/registers.hpp>
#include <vector>

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
