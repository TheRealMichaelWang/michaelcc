#include "linear/optimization/const_prop.hpp"

void michaelcc::linear::optimization::const_prop_pass::prescan(const translation_unit& unit) {
    for (const auto& [block_id, block] : unit.blocks) {
        for (const auto& instruction : block.instructions()) {
            if (auto init_register = dynamic_cast<linear::init_register*>(instruction.get())) {
                m_const_vregs[init_register->destination()] = init_register;
            }
        }
    }
}

bool michaelcc::linear::optimization::const_prop_pass::optimize(translation_unit& unit) {
    bool made_changes = false;

    instruction_pass pass(*this);
    for (auto& [block_id, block] : unit.blocks) {
        auto released_instructions = block.release_instructions();

        std::vector<std::unique_ptr<instruction>> new_instructions;
        new_instructions.reserve(released_instructions.size());
        for (auto& instruction : released_instructions) {
            std::unique_ptr<linear::instruction> new_instruction = pass(*instruction.get());

            if (new_instruction) {
                made_changes = true;
                new_instructions.emplace_back(std::move(new_instruction));
            } else {
                new_instructions.emplace_back(std::move(instruction));
            }
        }

        block.replace_instructions(std::move(new_instructions));
    }

    return made_changes;
}