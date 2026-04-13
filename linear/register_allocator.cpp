#include "linear/allocators/register_allocator.hpp"
#include "linear/ir.hpp"
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <deque>

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
    if (vreg_a.reg_class != vreg_b.reg_class) {
        return; // cannot connect registers of different classes (float will never interact with int and vice versa)
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
            if (dest_alloc_info.register_id.has_value()) {
                auto& node = ensure_node(it->get()->destination_register().value());

                if (!m_physical_to_family.contains(dest_alloc_info.register_id.value())) {
                    uint8_t family_id = static_cast<uint8_t>(m_register_families.size());
                    m_register_families.push_back(register_family{ 
                        family_id,
                        it->get()->destination_register().value().reg_size, 
                        it->get()->destination_register().value().reg_class, 
                        dest_alloc_info.register_id.value()
                    });
                    m_physical_to_family.insert({ dest_alloc_info.register_id.value(), family_id });
                }
                node.precolored_family_id = m_physical_to_family.at(dest_alloc_info.register_id.value());
            }
        }


        // marked clobbered registers as must use caller saved
        if (auto* call = dynamic_cast<const function_call*>(it->get())) {
            for (auto live_vreg : live_set) {
                ensure_node(live_vreg).prefer_caller_saved = true;
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

size_t michaelcc::linear::allocators::register_allocator::count_available_registers(michaelcc::linear::register_class reg_class, michaelcc::linear::word_size size){
    size_t count = 0;
    for (auto& physical_reg : m_translation_unit.platform_info.registers) {
        if (physical_reg.reg_class != reg_class || physical_reg.size != size || physical_reg.is_protected) { continue; }

        bool cannot_use = false;
        for (auto& mutually_exclusive_reg : physical_reg.mutually_exclusive_registers) {
            if (m_physical_to_family.contains(mutually_exclusive_reg)) {
                cannot_use = true;
                break;
            }
        }

        if (!cannot_use) {
            count++;
        }
    }

    return count;
}

std::vector<michaelcc::linear::virtual_register> michaelcc::linear::allocators::register_allocator::simplify() {
    // build the remaining nodes graph (without precolored nodes)
    // also remove precolored adjacent nodes where applicable
    std::unordered_map<virtual_register, inference_graph_node> remaining_nodes;
    for (const auto& [vreg, node] : m_inference_graph) {
        if (!node.precolored_family_id.has_value()) {
            inference_graph_node new_node(node); //copy node
            for (auto it = new_node.adjacent_vregs.begin(); it != new_node.adjacent_vregs.end();) {
                if (m_inference_graph.at(*it).precolored_family_id.has_value()) {
                    it = new_node.adjacent_vregs.erase(it);
                    new_node.degree--;
                }
                else {
                    ++it;
                }
            }

            remaining_nodes.insert({ vreg, node });
        }
    }

    std::deque<virtual_register> low_queue;
    auto push_if_low = [&](virtual_register vreg) {
        if (!remaining_nodes.contains(vreg)) { return; }
        int registers_available = count_available_registers(vreg.reg_class, vreg.reg_size);
        if (remaining_nodes.at(vreg).degree < registers_available) {
            low_queue.push_back(vreg);
        }
    };

    std::vector<virtual_register> select_stack;
    for (const auto& [vreg, node] : remaining_nodes) {
        push_if_low(vreg);
    }

    // remove/process trivially colorable nodes
    auto process_low_queue = [&]() { 
        while (!low_queue.empty()) {
            virtual_register vreg = low_queue.front();
            low_queue.pop_front();

            if (!remaining_nodes.contains(vreg)) { continue; }

            inference_graph_node current_node = remaining_nodes.at(vreg);
            select_stack.push_back(vreg);
            remaining_nodes.erase(vreg);

            for (auto adjacent_vreg : current_node.adjacent_vregs) {
                if (!remaining_nodes.contains(adjacent_vreg)) { continue; }
                remaining_nodes.at(adjacent_vreg).degree--;
                push_if_low(adjacent_vreg);
            }
        }
    };

    // handle all the spill candidates
    while (!remaining_nodes.empty()) {
        // pick the highest degree node as the spill candidate
        virtual_register spill_candidate = std::max_element(remaining_nodes.begin(), remaining_nodes.end(), 
        [](const auto& a, const auto& b) { return a.second.degree < b.second.degree; }
        )->first;

        inference_graph_node spill_node = remaining_nodes.at(spill_candidate);
        remaining_nodes.erase(spill_candidate);
        select_stack.push_back(spill_candidate);

        for (auto adjacent_vreg : spill_node.adjacent_vregs) {
            if (!remaining_nodes.contains(adjacent_vreg)) { continue; }
            remaining_nodes.at(adjacent_vreg).degree--;
            push_if_low(adjacent_vreg);
        }

        process_low_queue();
    }

    return select_stack;
}