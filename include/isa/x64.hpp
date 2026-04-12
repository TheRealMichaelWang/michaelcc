#ifndef MICHAELCC_ISA_X64_HPP
#define MICHAELCC_ISA_X64_HPP

#include "platform.hpp"
#include "linear/pass.hpp"

namespace michaelcc::isa::x64 {
    extern michaelcc::platform_info platform_info;
    extern std::vector<std::unique_ptr<michaelcc::linear::pass>> legalization_passes;
}

#endif