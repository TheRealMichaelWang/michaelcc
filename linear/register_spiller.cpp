#include "linear/allocators/register_spiller.hpp"
#include "linear/ir.hpp"
#include <cassert>
#include <utility>

michaelcc::linear::virtual_register michaelcc::linear::allocators::register_spiller::get_value(virtual_register vreg, std::vector<std::unique_ptr<instruction>>& new_instructions) const {
    if (!m_spilled_vregs.contains(vreg)) {
        return vreg;
    }

    assert(m_spill_address.contains(vreg));
    auto spill_address = m_spill_address.at(vreg);

    auto new_vreg = m_translation_unit.new_vreg(vreg.reg_size, vreg.reg_class);
    new_instructions.emplace_back(std::make_unique<load_memory>(new_vreg, spill_address.at(0), 0));
    return new_vreg;
}

void michaelcc::linear::allocators::register_spiller::spill_block(size_t block_id) {
    auto& block = m_translation_unit.blocks.at(block_id);
    
    auto released_instructions = block.release_instructions();
    std::vector<std::unique_ptr<instruction>> new_instructions;
    new_instructions.reserve(released_instructions.size());

    for (auto& instruction : released_instructions) {
        // replace spilled operand vregs
        replace_operands_transform transform(*this, new_instructions);
        std::unique_ptr<linear::instruction> new_instruction = transform(*instruction.get());
        if (!new_instruction) {
            new_instruction = std::move(instruction);
            assert(new_instruction != nullptr);
        }

        // save destination if necessary
        if (new_instruction->destination_register().has_value() && m_spilled_vregs.contains(new_instruction->destination_register().value())) {
            auto dest_vreg = new_instruction->destination_register().value();
            auto address_vreg = m_translation_unit.new_vreg(m_translation_unit.platform_info.pointer_size, MICHAELCC_REGISTER_CLASS_INTEGER);
            new_instructions.emplace_back(std::make_unique<alloca_instruction>(
                address_vreg, 
                static_cast<size_t>(dest_vreg.reg_size) / 8,
                static_cast<size_t>(dest_vreg.reg_size) / 8
            ));
            new_instructions.emplace_back(std::move(new_instruction));
            new_instructions.emplace_back(std::make_unique<store_memory>(
                dest_vreg,
                address_vreg,
                0
            ));

            m_spill_address.insert({ dest_vreg, { address_vreg } });
        } else {
            new_instructions.emplace_back(std::move(new_instruction));
        }
    }
}

void michaelcc::linear::allocators::register_spiller::spill() {
    for (const auto& [block_id, block] : m_translation_unit.blocks) {
        spill_block(block_id);
    }
}