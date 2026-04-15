#ifndef MICHAELCC_LINEAR_ALLOCATORS_FRAME_ALLOCATOR_HPP
#define MICHAELCC_LINEAR_ALLOCATORS_FRAME_ALLOCATOR_HPP

#include "linear/ir.hpp"
#include <unordered_map>
#include <utility>

namespace michaelcc::linear::allocators {
    // Converts all alloca instructions into actual references to memory locations on the stack
    // Aparently not all platforms have stacks grow downwards (strange?)
    class frame_allocator final {
    private:
        linear::translation_unit& translation_unit;

        // map of block id to current stack offset and frame pointer virtual register
        // NOTE: offset is SUBRACTED from the frame pointer
        std::unordered_map<size_t, std::pair<size_t, linear::virtual_register>> function_to_frame_pointer;

        void allocate_block(linear::function_definition* function, size_t block_id);
    public:
        frame_allocator(linear::translation_unit& translation_unit);

        void allocate();

        size_t get_reserved_stack_space(size_t function_id) const {
            return function_to_frame_pointer.at(function_id).first;
        }
    };

}

#endif