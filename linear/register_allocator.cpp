#include "linear/allocators/register_allocator.hpp"
#include <unordered_set>

std::unordered_set<michaelcc::linear::virtual_register> michaelcc::linear::allocators::register_allocator::compute_defined_vregs(size_t block_id) {
    std::unordered_set<michaelcc::linear::virtual_register> defined_vregs;
    for (const auto& instruction : m_translation_unit.blocks.at(block_id).instructions()) {
        if (instruction->destination_register().has_value()) {
            defined_vregs.insert(instruction->destination_register().value());
        }
    }
    return defined_vregs;
}

std::unordered_set<michaelcc::linear::virtual_register> michaelcc::linear::allocators::register_allocator::compute_used_vregs(size_t block_id, const std::unordered_set<virtual_register>& defined_vregs) {
    std::unordered_set<michaelcc::linear::virtual_register> used_vregs;
    for (const auto& instruction : m_translation_unit.blocks.at(block_id).instructions()) {
        if (dynamic_cast<const phi_instruction*>(instruction.get())) {
            continue;
        }
        
        for (const auto& operand : instruction->operand_registers()) {
            if (!defined_vregs.contains(operand)) {
                used_vregs.insert(operand);
            }
        }
    }
    return used_vregs;
}

std::vector<const michaelcc::linear::phi_instruction*> michaelcc::linear::allocators::register_allocator::compute_phi_instructions(size_t block_id) {
    std::vector<const phi_instruction*> phi_instructions;
    for (const auto& instruction : m_translation_unit.blocks.at(block_id).instructions()) {
        if (auto* phi = dynamic_cast<const phi_instruction*>(instruction.get())) {
            phi_instructions.push_back(phi);
        }
    }
    return phi_instructions;
}

bool michaelcc::linear::allocators::register_allocator::compute_block_liveliness(size_t block_id) {
    auto& block_info = compute_block_info(block_id);

    // live out is the union of the live in of all successor blocks
    std::unordered_set<virtual_register> computed_live_out;
    for (size_t succ_id : m_translation_unit.blocks.at(block_id).successor_block_ids()) {
        // add the standard live_in of the successor (minus phi defs, plus non-phi live-in)
        if (m_block_liveliness.contains(succ_id)) {
            computed_live_out.insert(
                m_block_liveliness.at(succ_id).live_in.begin(),
                m_block_liveliness.at(succ_id).live_in.end()
            );
        }

        auto& succ_block_info = compute_block_info(succ_id);
        // add phi operands from this specific edge
        for (const auto& phi : succ_block_info.phi_instructions) {
            for (const auto& val : phi->values()) {
                if (val.block_id == block_id) {
                    computed_live_out.insert(val.vreg);
                }
            }
        }
    }

    // live in is the union of block.used_vregs and (computed_live_out - block.defined_vregs)
    std::unordered_set<virtual_register> computed_live_in(block_info.used_vregs);
    for (virtual_register vreg : computed_live_out) {
        if (!block_info.defined_vregs.contains(vreg)) {
            computed_live_in.insert(vreg);
        }
    }

    if (!m_block_liveliness.contains(block_id)) {
        m_block_liveliness.insert({ block_id, block_liveliness{ computed_live_in, computed_live_out } });
        return true;
    }
    else {
        bool changed = false;
        changed |= update_set(m_block_liveliness.at(block_id).live_in, computed_live_in);
        changed |= update_set(m_block_liveliness.at(block_id).live_out, computed_live_out);
        return changed;
    }
}

void michaelcc::linear::allocators::register_allocator::compute_all_block_liveliness() {
    bool changed;
    do {
        changed = false;
        for (const auto& [block_id, _] : m_translation_unit.blocks) {
            changed |= compute_block_liveliness(block_id);
        }
    } while (changed);
}