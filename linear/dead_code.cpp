#include "linear/optimization/dead_code.hpp"
#include "linear/registers.hpp"
#include <unordered_map>
#include <queue>

void michaelcc::linear::optimization::dead_code_pass::prescan(const translation_unit& unit) {
    // prowl through the IR to find all the definitions of virtual registers
    std::unordered_map<virtual_register, instruction*> m_vreg_defintions;

    std::queue<instruction*> instructions_to_traverse;
    for (const auto& [block_id, block] : unit.blocks) {
        for (const auto& instruction : block.instructions()) {
            if (instruction->destination_register().has_value()) {
                m_vreg_defintions[instruction->destination_register().value()] = instruction.get();

                auto alloc_info = unit.register_allocator.get_alloc_information(instruction->destination_register().value());
                if (alloc_info.register_id.has_value()) {
                    instructions_to_traverse.push(instruction.get());
                }
            }
            if (instruction->has_side_effects()) {
                instructions_to_traverse.push(instruction.get());
            }
        }
    }

    used_instructions.clear();
    while (!instructions_to_traverse.empty()) {
        auto instruction = instructions_to_traverse.front();
        instructions_to_traverse.pop();

        if (used_instructions.contains(instruction)) {
            continue;
        }
        used_instructions.insert(instruction);

        for (const auto& operand : instruction->operand_registers()) {
            auto def_ins = m_vreg_defintions.find(operand);
            if (def_ins != m_vreg_defintions.end() && !used_instructions.contains(def_ins->second)) {
                instructions_to_traverse.push(def_ins->second);
            }
        } 
    }
}

bool michaelcc::linear::optimization::dead_code_pass::optimize(translation_unit& unit) {
    bool made_changes = false;
    for (auto& [block_id, block] : unit.blocks) {
        std::vector<std::unique_ptr<instruction>> new_instructions;
        new_instructions.reserve(block.instructions().size());

        auto released_instructions = block.release_instructions();
        for (auto& instruction : released_instructions) {
            // is the destination is unsured we remove the instruction
            if (!used_instructions.contains(instruction.get())) {
                made_changes = true;
                continue;
            }
            new_instructions.emplace_back(std::move(instruction));
        }
        block.replace_instructions(std::move(new_instructions));
    }

    return made_changes;
}