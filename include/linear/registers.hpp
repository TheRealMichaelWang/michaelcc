#ifndef MICHAELCC_LINEAR_REGISTERS_HPP
#define MICHAELCC_LINEAR_REGISTERS_HPP

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <cstdint>

namespace michaelcc {
    namespace linear {
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
    }
}

#endif