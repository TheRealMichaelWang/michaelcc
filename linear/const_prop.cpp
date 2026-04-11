#include "linear/optimization/const_prop.hpp"
#include <memory>

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

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::a_instruction& node) {
    auto const_lhs = m_pass.get_const_value(node.operand_a());
    auto const_rhs = m_pass.get_const_value(node.operand_b());
    if (const_lhs.has_value() && const_rhs.has_value()) {
        switch (node.type()) {
        case michaelcc::linear::a_instruction_type::MICHAELCC_LINEAR_A_ADD: 
            
        default: return nullptr;
        }
    }
    return nullptr;
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::copy_instruction& node) {
    auto const_value = m_pass.get_const_value(node.source());
    if (const_value.has_value()) {
        return std::make_unique<michaelcc::linear::init_register>(node.destination(), const_value.value());
    }
    return nullptr;
}