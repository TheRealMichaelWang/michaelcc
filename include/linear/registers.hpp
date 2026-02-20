#ifndef MICHAELCC_LINEAR_REGISTERS_HPP
#define MICHAELCC_LINEAR_REGISTERS_HPP

#include <string>
#include <optional>

namespace michaelcc {
    namespace linear {
        struct alloc_information {
            std::optional<std::string> name = std::nullopt;
            bool must_use_register = false;
        };
    }
}

#endif