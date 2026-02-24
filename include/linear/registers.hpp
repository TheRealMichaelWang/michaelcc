#ifndef MICHAELCC_LINEAR_REGISTERS_HPP
#define MICHAELCC_LINEAR_REGISTERS_HPP

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace michaelcc {
    namespace linear {
        struct virtual_register { 
            size_t id; size_t size_bits; 

            bool operator==(const virtual_register& reg) const { return reg.id == id; }
        };

        struct register_info {
            uint8_t id;
            
            std::string name;
            std::string description;

            std::vector<uint8_t> mutually_exclusive_registers;
            size_t size_bits;
        };

        struct alloc_information {
            std::optional<std::string> name = std::nullopt;
            bool must_use_register = false;

            std::optional<uint8_t> register_id = std::nullopt;
        };

        class register_allocator {
        private:
            size_t m_next_vreg_id = 0;
            std::unordered_map<size_t, std::shared_ptr<linear::alloc_information>> m_vreg_alloc_information;
            
        public:
            linear::virtual_register new_vreg(size_t size_bits) {
                size_t id = m_next_vreg_id++;
                return { id, size_bits };
            }

            linear::alloc_information get_alloc_information(linear::virtual_register vreg) {
                if (m_vreg_alloc_information.find(vreg.id) == m_vreg_alloc_information.end()) {
                    return linear::alloc_information{
                        .name = std::nullopt,
                        .must_use_register = false,
                        .register_id = std::nullopt
                    };
                }
                return *m_vreg_alloc_information.at(vreg.id);
            }

            void set_alloc_information(linear::virtual_register vreg, std::shared_ptr<linear::alloc_information> alloc_information) {
                m_vreg_alloc_information[vreg.id] = alloc_information;
            }
        };
    }
}

#endif