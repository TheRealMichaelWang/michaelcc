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

michaelcc::linear::allocators::register_allocator::inference_graph_node& michaelcc::linear::allocators::register_allocator::ensure_node(virtual_register vreg) {
    if (!m_inference_graph.contains(vreg)) {
        m_inference_graph.insert({ vreg, inference_graph_node{.vreg = vreg, .degree = 0 } });
    }
    return m_inference_graph.at(vreg);
}

void michaelcc::linear::allocators::register_allocator::add_edge(virtual_register vreg_a, virtual_register vreg_b) {
    if (vreg_a.reg_class != vreg_b.reg_class || vreg_a.reg_size != vreg_b.reg_size) {
        return; // cannot connect registers of different classes or sizes (float will never interact with int and vice versa)
    }

    auto& node_a = ensure_node(vreg_a);
    if (node_a.adjacent_vregs.contains(vreg_b)) {
        return; // no duplicate edges
    }

    auto& node_b = ensure_node(vreg_b);
    node_a.adjacent_vregs.insert(vreg_b);
    node_b.adjacent_vregs.insert(vreg_a);
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

            auto dest_alloc_info = m_translation_unit.register_allocator.get_alloc_information(it->get()->destination_register().value());
            if (dest_alloc_info.register_id.has_value() || dest_alloc_info.must_use_register) {
                auto node = ensure_node(it->get()->destination_register().value());
                node.must_use_register = true;
                node.must_use_register_id = dest_alloc_info.register_id;
            }
        }


        // marked clobbered registers as must use caller saved
        if (auto* call = dynamic_cast<const function_call*>(it->get())) {
            for (auto live_vreg : live_set) {
                ensure_node(live_vreg).must_use_caller_saved = true;
            }
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