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
        for (const auto& operand : instruction->operand_registers()) {
            if (!defined_vregs.contains(operand)) {
                used_vregs.insert(operand);
            }
        }
    }
    return used_vregs;
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

void michaelcc::linear::allocators::register_allocator::add_edge(virtual_register vreg_a, virtual_register vreg_b) {
    auto ensure_edge = [this](virtual_register vreg) {
        if (!m_inference_graph.contains(vreg)) {
            m_inference_graph.insert({ vreg, inference_graph_node{.vreg = vreg, .degree = 0 } });
        }
        return m_inference_graph.at(vreg);
    };

    auto node_a = ensure_edge(vreg_a);
    auto node_b = ensure_edge(vreg_b);
    node_a.adjacent_node_ids.push_back(vreg_b);
    node_b.adjacent_node_ids.push_back(vreg_a);
    node_a.degree++;
    node_b.degree++;
}

void michaelcc::linear::allocators::register_allocator::build_inference_graph(size_t block_id) {
    auto& block_info = compute_block_info(block_id);
    auto& block = m_translation_unit.blocks.at(block_id);

    // live set starts as live out
    std::unordered_set<virtual_register> live_set(m_block_liveliness.at(block_id).live_out);
    for (auto it = block.instructions().rbegin(); it != block.instructions().rend(); ++it) {
        if (it->get()->destination_register().has_value()) {
            for (auto vreg : live_set) {
                add_edge(it->get()->destination_register().value(), vreg);
            }
            live_set.erase(it->get()->destination_register().value());
        }
        for (auto operand : it->get()->operand_registers()) {
            live_set.insert(operand);
        }
    }
}

void michaelcc::linear::allocators::register_allocator::build_inference_graph() {
    for (const auto& [block_id, _] : m_translation_unit.blocks) {
        build_inference_graph(block_id);
    }
}