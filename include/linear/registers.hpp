#ifndef MICHAELCC_LINEAR_REGISTERS_HPP
#define MICHAELCC_LINEAR_REGISTERS_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <cstdint>

namespace michaelcc {
    namespace linear {
        enum word_size {
            MICHAELCC_WORD_SIZE_BYTE = 8,
            MICHAELCC_WORD_SIZE_UINT16 = 16,
            MICHAELCC_WORD_SIZE_UINT32 = 32,
            MICHAELCC_WORD_SIZE_UINT64 = 64
        };

        union register_word {
            uint8_t ubyte;
            uint16_t uint16;
            uint32_t uint32;
            uint64_t uint64;
            int8_t sbyte;
            int16_t int16;
            int32_t int32;
            int64_t int64;
            float float32;
            double float64;
        };

        enum register_class {
            MICHAELCC_REGISTER_CLASS_INTEGER,
            MICHAELCC_REGISTER_CLASS_FLOATING_POINT
        };

        struct virtual_register { 
            size_t id; word_size reg_size; register_class reg_class;

            bool operator==(const virtual_register& reg) const { return reg.id == id; }
        };

        using register_t = uint8_t;

        struct register_info {
            register_t id;
            
            std::string name;
            std::string description;

            std::vector<register_t> mutually_exclusive_registers;
            word_size size;
            register_class reg_class;

            bool is_caller_saved;
            bool is_callee_saved;
            bool is_protected = false;
        };
    }
}

namespace std {
    template<>
    struct hash<michaelcc::linear::virtual_register> {
        size_t operator()(const michaelcc::linear::virtual_register& reg) const {
            return std::hash<size_t>()(reg.id);
        }
    };
}

#endif