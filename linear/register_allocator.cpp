#include "linear/allocators/register_allocator.hpp"
#include "linear/ir.hpp"
#include <algorithm>
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
    if (vreg_a == vreg_b || vreg_a.reg_class != vreg_b.reg_class) {
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
        }


        // marked clobbered registers as must use caller saved
        if (auto* call = dynamic_cast<const function_call*>(it->get())) {
            for (auto live_vreg : live_set) {
                ensure_node(live_vreg).must_avoid_caller_saved = true;
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

    std::vector<register_t> potential_registers;
    for (auto& physical_reg : m_translation_unit.platform_info.registers) {
        if (physical_reg.reg_class != reg_class || physical_reg.size < size || physical_reg.is_protected) { continue; }
        potential_registers.push_back(physical_reg.id);
    }

    std::sort(potential_registers.begin(), potential_registers.end(), [&](register_t a, register_t b) {
        size_t a_size_diff = m_translation_unit.platform_info.get_register_info(a).size - size;
        size_t b_size_diff = m_translation_unit.platform_info.get_register_info(b).size - size;
        return a_size_diff < b_size_diff;
    });

    // first count exact fits
    std::unordered_set<register_t> disallowed_registers;
    for (auto& potential_register : potential_registers) {
        if (disallowed_registers.contains(potential_register)) { continue; }
        for (auto& mutually_exclusive_reg : m_translation_unit.platform_info.get_register_info(potential_register).mutually_exclusive_registers) {
            disallowed_registers.insert(mutually_exclusive_reg);
        }
        count++;
    }

    return count;
}

std::vector<michaelcc::linear::virtual_register> michaelcc::linear::allocators::register_allocator::simplify() {
    // build the remaining nodes graph (exclude already precolored/fixed nodes)
    std::unordered_map<virtual_register, inference_graph_node> remaining_nodes;
    for (const auto& [vreg, node] : m_inference_graph) {
        auto alloc_info = m_translation_unit.register_allocator.get_alloc_information(vreg);
        if (alloc_info.register_id.has_value()) {
            continue;
        }

        inference_graph_node new_node(node);
        for (auto it = new_node.adjacent_vregs.begin(); it != new_node.adjacent_vregs.end();) {
            auto alloc_info_adjacent = m_translation_unit.register_allocator.get_alloc_information(*it);
            if (alloc_info_adjacent.register_id.has_value()) {
                it = new_node.adjacent_vregs.erase(it);
            }
            else {
                ++it;
            }
        }
        remaining_nodes.insert({ vreg, new_node });
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

    // first drain all trivially colorable nodes
    process_low_queue();

    // handle all the spill candidates
    while (!remaining_nodes.empty()) {
        // pick the highest degree node as the spill candidate
        virtual_register spill_candidate = std::max_element(remaining_nodes.begin(), remaining_nodes.end(), 
        [this](const auto& a, const auto& b) { 
            const auto& na = a.second;
            const auto& nb = b.second;
            // Prefer choosing spillable nodes as spill candidates.
            // if a is must_use_register and b is not, a is "worse candidate".
            auto alloc_info_a = m_translation_unit.register_allocator.get_alloc_information(a.first);
            auto alloc_info_b = m_translation_unit.register_allocator.get_alloc_information(b.first);
            if (alloc_info_a.must_use_register != alloc_info_b.must_use_register) {
                return alloc_info_a.must_use_register; // true => lower priority
            }

            // Otherwise, prefer nodes with lower degree.
            return na.degree < nb.degree;
         }
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

std::vector<michaelcc::linear::virtual_register> michaelcc::linear::allocators::register_allocator::select(const std::vector<virtual_register>& select_stack) {
    std::vector<virtual_register> spilled_vregs;

    for (auto it = select_stack.rbegin(); it != select_stack.rend(); ++it) {
        auto& node = m_inference_graph.at(*it);
        virtual_register vreg = node.vreg;

        // build a list of forbidden families
        std::unordered_set<register_t> forbidden_families;
        for (auto adjacent_vreg : node.adjacent_vregs) {
            auto alloc_info_adjacent = m_translation_unit.register_allocator.get_alloc_information(adjacent_vreg);
            if (!alloc_info_adjacent.register_id.has_value()) { continue; }

            register_t forbidden_family = alloc_info_adjacent.register_id.value();
            forbidden_families.insert(forbidden_family);

            // remeber to mark all the mutually exclusive registers as forbidden
            auto register_info = m_translation_unit.platform_info.get_register_info(forbidden_family);
            for (auto mutually_exclusive_reg : register_info.mutually_exclusive_registers) {
                forbidden_families.insert(mutually_exclusive_reg);
            }
        }

        // compare two physical registers by their suitability for the given vreg (specifically if a family is better than b)
        auto better_fit = [&](size_t physical_reg_id_a, size_t physical_reg_id_b) -> bool {
            auto a_register_info = m_translation_unit.platform_info.get_register_info(physical_reg_id_a);
            auto b_register_info = m_translation_unit.platform_info.get_register_info(physical_reg_id_b);

            bool a_fits_exactly = a_register_info.size == vreg.reg_size;
            bool b_fits_exactly = b_register_info.size == vreg.reg_size;
            
            // prioritize exact fits over partial fits
            if (a_fits_exactly != b_fits_exactly) { return a_fits_exactly; }


            if (node.must_avoid_caller_saved) {
                if (a_register_info.is_caller_saved != b_register_info.is_caller_saved) { return !a_register_info.is_caller_saved; }
            }

            // prioritize the smallest register that fits better
            if (a_register_info.size != b_register_info.size) { return a_register_info.size < b_register_info.size; }

            // prioritize registers with fewer mutually exclusive registers
            return a_register_info.mutually_exclusive_registers.size() < b_register_info.mutually_exclusive_registers.size(); 
        };
        

        // find if any existing families can accomodate this vreg
        std::optional<register_t> best_fit = std::nullopt;
        for (auto physical_reg : m_translation_unit.platform_info.registers) {
            if (forbidden_families.contains(physical_reg.id)) { continue; }
            if (physical_reg.reg_class != vreg.reg_class || physical_reg.size < vreg.reg_size || physical_reg.is_protected) { continue; } // families must be of the same class

            if (!best_fit.has_value() || better_fit(physical_reg.id, best_fit.value())) {
                best_fit = physical_reg.id;
            }
        }

        if (best_fit.has_value()) { //we succesfully color the vreg
            m_translation_unit.register_allocator.set_alloc_information(vreg, std::make_shared<alloc_information>(alloc_information{
                .register_id = best_fit.value()
            }));
        } else {
            auto alloc_info = m_translation_unit.register_allocator.get_alloc_information(vreg);
            if (alloc_info.must_use_register) {
                throw std::runtime_error("Register allocation failed: Cannot spill a must use register.");
            }
            spilled_vregs.push_back(vreg);
        }
    }
    return spilled_vregs;
}