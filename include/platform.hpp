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

        linear::register_t return_register_int8_id;
        linear::register_t return_register_int16_id;
        linear::register_t return_register_int32_id;
        linear::register_t return_register_int64_id;
        linear::register_t return_register_float32_id;
        linear::register_t return_register_float64_id;
        
        std::vector<linear::register_info> registers;

        linear::register_t get_return_register_id(linear::register_class reg_class, linear::word_size size) const {
            if (reg_class == linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER) {
                switch (size) {
                    case linear::word_size::MICHAELCC_WORD_SIZE_BYTE: return return_register_int8_id;
                    case linear::word_size::MICHAELCC_WORD_SIZE_UINT16: return return_register_int16_id;
                    case linear::word_size::MICHAELCC_WORD_SIZE_UINT32: return return_register_int32_id;
                    case linear::word_size::MICHAELCC_WORD_SIZE_UINT64: return return_register_int64_id;
                    default: return return_register_int64_id;
                }
            } else {
                switch (size) {
                    case linear::word_size::MICHAELCC_WORD_SIZE_UINT32: return return_register_float32_id;
                    case linear::word_size::MICHAELCC_WORD_SIZE_UINT64: return return_register_float64_id;
                    default: return return_register_float64_id;
                }
            }
        }

        const linear::register_info& get_register_info(linear::register_t id) const {
            return registers.at(static_cast<int>(id));
        }

        linear::word_size max_register_size() const {
            return std::max_element(registers.begin(), registers.end(), [](const linear::register_info& a, const linear::register_info& b) {
                return static_cast<size_t>(a.size) < static_cast<size_t>(b.size);
            })->size;
        }
    };
}

#endif
