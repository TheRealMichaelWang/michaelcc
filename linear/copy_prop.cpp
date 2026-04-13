#include "linear/optimization/copy_prop.hpp"
#include "linear/dominators.hpp"
#include "linear/ir.hpp"
#include <utility>

void michaelcc::linear::optimization::copy_prop_pass::prescan(const translation_unit& unit) {
    // prowl through the IR to find all the definitions of virtual registers
    for (const auto& [block_id, block] : unit.blocks) {
        for (const auto& instruction : block.instructions()) {
            if (instruction->destination_register().has_value()) {
                m_instruction_map.insert({
                    instruction->destination_register().value(),
                    block_id
                });
            }
        }
    }
}

bool michaelcc::linear::optimization::copy_prop_pass::optimize(translation_unit& unit) {
    bool made_changes = false;
    std::unordered_map<virtual_register, virtual_register> m_vreg_substitutions;
    for (auto& [block_id, block] : unit.blocks) {
        std::vector<std::unique_ptr<instruction>> released_instructions = block.release_instructions();
        std::vector<std::unique_ptr<instruction>> new_instructions;
        new_instructions.reserve(released_instructions.size());

        auto push_instruction = [&](std::unique_ptr<instruction>&& instruction) {
            replace_operands_transform transform(m_vreg_substitutions);
            auto new_instruction = transform(*instruction.get());
            if (new_instruction) {
                new_instructions.emplace_back(std::move(new_instruction));
            } else {
                new_instructions.emplace_back(std::move(instruction));
            }
        };

        for (auto& instruction : released_instructions) {
            if (auto* copy = dynamic_cast<linear::c_instruction*>(instruction.get())) {
                if (copy->type() != MICHAELCC_LINEAR_C_COPY_INIT) {
                    push_instruction(std::move(instruction));
                    continue;
                }
                
                size_t source_block_id = m_instruction_map.at(copy->source());
                assert(copy->source().reg_size == copy->destination().reg_size);
                assert(copy->source().reg_class == copy->destination().reg_class);

                if (!linear::is_dominated_by(unit, source_block_id, block_id)) {
                    push_instruction(std::move(instruction));
                    continue;
                }

                // if the destination is a physical register, we must keep that vreg
                if (unit.vreg_colors.contains(copy->destination())) {
                    // source instruction must be from the same block
                    if (source_block_id != block_id) {
                        push_instruction(std::move(instruction));
                        continue;
                    }

                    // add the register allocation information
                    unit.vreg_colors[copy->source()] = unit.vreg_colors.at(copy->destination());
                    unit.free_vreg(copy->destination());
                }

                // insert the substitution into the map
                m_vreg_substitutions.insert({
                    copy->destination(),
                    copy->source()
                });

                made_changes = true;
            }
            else {
                push_instruction(std::move(instruction));
            }
        }
        block.replace_instructions(std::move(new_instructions));
    }
    return made_changes;
}
