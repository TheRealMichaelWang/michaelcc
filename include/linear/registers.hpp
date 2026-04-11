#ifndef MICHAELCC_LINEAR_REGISTERS_HPP
#define MICHAELCC_LINEAR_REGISTERS_HPP

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <memory>
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

        struct register_info {
            uint8_t id;
            
            std::string name;
            std::string description;

            std::vector<uint8_t> mutually_exclusive_registers;
            word_size size;
            register_class reg_class;
        };

        struct alloc_information {
            bool must_use_register = false;
            std::optional<uint8_t> register_id = std::nullopt;
        };

        class register_allocator {
        private:
            size_t m_next_vreg_id = 0;
            std::unordered_map<size_t, std::shared_ptr<linear::alloc_information>> m_vreg_alloc_information;
            
        public:
            linear::virtual_register new_vreg(word_size reg_size, register_class reg_class) {
                size_t id = m_next_vreg_id++;
                return { id, reg_size, reg_class };
            }

            linear::alloc_information get_alloc_information(linear::virtual_register vreg) const {
                if (m_vreg_alloc_information.find(vreg.id) == m_vreg_alloc_information.end()) {
                    return linear::alloc_information{
                        .must_use_register = false,
                        .register_id = std::nullopt
                    };
                }
                return *m_vreg_alloc_information.at(vreg.id);
            }

            void set_alloc_information(linear::virtual_register vreg, std::shared_ptr<linear::alloc_information> alloc_information);
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