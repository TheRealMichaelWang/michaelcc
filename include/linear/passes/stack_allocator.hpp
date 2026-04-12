#ifndef MICHAELCC_LINEAR_PASSES_STACK_ALLOCATOR_HPP
#define MICHAELCC_LINEAR_PASSES_STACK_ALLOCATOR_HPP

#include "linear/pass.hpp"

namespace michaelcc::linear::passes {
    // Converts all alloca instructions into actual references to memory locations on the stack
    class stack_allocator_pass final : public michaelcc::linear::pass {
    private:
    
    public:
        void prescan(const michaelcc::linear::translation_unit& unit) override;
        bool optimize(michaelcc::linear::translation_unit& unit) override;

        void reset() override { }
    };

}