#ifndef MICHAELCC_LINEAR_PASSES_STACK_ALLOCATOR_HPP
#define MICHAELCC_LINEAR_PASSES_STACK_ALLOCATOR_HPP

#include "linear/ir.hpp"
#include <unordered_map>

namespace michaelcc::linear::passes {
    // Converts all alloca instructions into actual references to memory locations on the stack
    // Aparently not all platforms have stacks grow downwards (strange?)
    class frame_allocator final {
    private:
        linear::translation_unit& translation_unit;
        size_t function_id;

        // map of block id to current stack offset
        std::unordered_map<size_t, size_t> block_to_stack_offset;

        void allocate_block(size_t block_id);
    public:
        frame_allocator(linear::translation_unit& translation_unit, size_t function_id) 
            : translation_unit(translation_unit), function_id(function_id) { 
                block_to_stack_offset.emplace(translation_unit.function_definitions.at(function_id)->entry_block_id(), 0);
            }

        void allocate();
    };

}

#endif