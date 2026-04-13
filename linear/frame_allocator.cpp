#include "linear/allocators/frame_allocator.hpp"
#include "linear/ir.hpp"

michaelcc::linear::allocators::frame_allocator::frame_allocator(linear::translation_unit& translation_unit)  : translation_unit(translation_unit) { 
    for (const auto& function : translation_unit.function_definitions) {
        auto frame_pointer_vreg = translation_unit.new_vreg(
            translation_unit.platform_info.pointer_size, 
            linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
        );
        translation_unit.vreg_colors[frame_pointer_vreg] = translation_unit.platform_info.frame_pointer_register_id;
        function_to_frame_pointer.emplace(function.get(), std::make_pair(0, frame_pointer_vreg));
    }
}

void michaelcc::linear::allocators::frame_allocator::allocate_block(linear::function_definition* function, size_t block_id) {
    auto& block = translation_unit.blocks.at(block_id);
    
    auto [current_offset, frame_pointer_vreg] = function_to_frame_pointer.at(function);

    auto released_instructions = block.release_instructions();
    std::vector<std::unique_ptr<instruction>> new_instructions;
    new_instructions.reserve(released_instructions.size());
    for (auto& instruction : released_instructions) {
        if (auto alloca_instruction = dynamic_cast<linear::alloca_instruction*>(instruction.get())) {
            current_offset += (current_offset % alloca_instruction->alignment()) + alloca_instruction->size_bytes();

            new_instructions.emplace_back(std::make_unique<linear::a2_instruction>(
                linear::MICHAELCC_LINEAR_A_SUBTRACT,
                alloca_instruction->destination(),
                frame_pointer_vreg,
                current_offset
            ));
        }
        else {
            new_instructions.emplace_back(std::move(instruction));
        }
    }

    block.replace_instructions(std::move(new_instructions));
    function_to_frame_pointer.at(function) = std::make_pair(current_offset, frame_pointer_vreg);

    for (size_t successor : block.successor_block_ids()) {
        allocate_block(function, successor);
    }
}

void michaelcc::linear::allocators::frame_allocator::allocate() {
    for (const auto& function : translation_unit.function_definitions) {
        allocate_block(function.get(), function->entry_block_id());
    }
}