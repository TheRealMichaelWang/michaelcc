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
                    std::make_pair(instruction.get(), block_id)
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
                
                auto source_instruction = m_instruction_map.at(copy->source());
                assert(source_instruction.first->destination_register().has_value());
                assert(source_instruction.first->destination_register().value().reg_size == copy->destination().reg_size);
                assert(source_instruction.first->destination_register().value().reg_class == copy->destination().reg_class);

                if (!linear::is_dominated_by(unit, source_instruction.second, block_id)) {
                    push_instruction(std::move(instruction));
                    continue;
                }

                auto reg_alloc_info = unit.register_allocator.get_alloc_information(copy->destination());
                // if the destination is a physical register, we must keep that vreg
                if (reg_alloc_info.register_id.has_value()) {
                    // source instruction must be from the same block
                    if (source_instruction.second != block_id) {
                        push_instruction(std::move(instruction));
                        continue;
                    }

                    // add the register allocation information
                    unit.register_allocator.set_alloc_information(copy->source(), std::make_shared<alloc_information>(alloc_information{
                        .register_id = reg_alloc_info.register_id.value()
                    }));
                    unit.register_allocator.free_vreg(copy->destination());
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
