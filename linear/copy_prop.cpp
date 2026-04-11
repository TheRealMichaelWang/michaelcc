#include "linear/optimization/copy_prop.hpp"
#include "linear/ir.hpp"
#include <utility>

void michaelcc::linear::optimization::copy_prop_pass::prescan(const translation_unit& unit) {
    // prowl through the IR to find all the definitions of virtual registers
    std::unordered_map<virtual_register, std::pair<instruction*, size_t>> m_instruction_map;
    for (const auto& [block_id, block] : unit.blocks) {
        for (const auto& instruction : block.instructions()) {
            if (instruction->destination_register().has_value()) {
                m_instruction_map.insert({
                    instruction->destination_register().value(),
                    std::make_pair(instruction.get(), block_id)
                });
            }
        }
    }

    // find all copy inits and check if both definition of source and copy instruction are from the same block
    for (const auto& [block_id, block] : unit.blocks) {
        for (const auto& instruction : block.instructions()) {
            if (auto* copy = dynamic_cast<linear::c_instruction*>(instruction.get())) {
                if (copy->type() != MICHAELCC_LINEAR_C_COPY_INIT) {
                    continue;
                }

                // source instruction must be from the same block
                auto source_instruction = m_instruction_map.at(copy->source());
                if (source_instruction.second != block_id) {
                    continue;
                }

                assert(source_instruction.first->destination_register().has_value());
                assert(source_instruction.first->destination_register().value().reg_size == copy->destination().reg_size);
                assert(source_instruction.first->destination_register().value().reg_class == copy->destination().reg_class);

                m_new_definitions.insert({
                    source_instruction.first,
                    copy->destination()
                });
                m_dead_instructions.insert(copy);
            }
        }
    }
}

bool michaelcc::linear::optimization::copy_prop_pass::optimize(translation_unit& unit) {
    bool made_changes = false;
    for (auto& [block_id, block] : unit.blocks) {
        std::vector<std::unique_ptr<instruction>> released_instructions = block.release_instructions();
        std::vector<std::unique_ptr<instruction>> new_instructions;
        new_instructions.reserve(released_instructions.size());

        for (auto& instruction : released_instructions) {
            if (m_new_definitions.contains(instruction.get())) {
                made_changes = true;

                replace_dest_transform transform(m_new_definitions.at(instruction.get()));
                std::unique_ptr<linear::instruction> new_instruction = transform(*instruction.get());
                if (new_instruction) {
                    new_instructions.emplace_back(std::move(new_instruction));
                } else {
                    new_instructions.emplace_back(std::move(instruction));
                }
            }
            else if (m_dead_instructions.contains(instruction.get())) {
                made_changes = true;
                continue;
            }
            else {
                new_instructions.emplace_back(std::move(instruction));
            }
        }
        block.replace_instructions(std::move(new_instructions));
    }
    return made_changes;
}
