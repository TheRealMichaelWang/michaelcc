#include "linear/flatten.hpp"
#include "linear/ir.hpp"
#include "logic/ir.hpp"
#include "logic/type_info.hpp"
#include <bit>
#include <memory>
#include <sstream>
#include <unordered_set>

using namespace michaelcc;

logic_lowerer::block_var_ctx logic_lowerer::reconcile_var_regs(const std::vector<size_t>& incoming_block_ids) {
    block_var_ctx result;

    std::unordered_set<std::shared_ptr<logic::variable>> seen_variables;
    for (size_t block_id : incoming_block_ids) {
        const auto& block_var_ctx = m_finished_block_var_ctx.at(block_id);
        for (const auto& [variable, vregs] : block_var_ctx.m_variable_to_vreg) {
            seen_variables.insert(variable);
        }
    }

    for (const std::shared_ptr<logic::variable>& variable : seen_variables) {
        std::vector<linear::var_info> vregs;
        for (size_t block_id : incoming_block_ids) {
            const auto& block_var_ctx = m_finished_block_var_ctx.at(block_id);
            auto it = block_var_ctx.m_variable_to_vreg.find(variable);
            if (it != block_var_ctx.m_variable_to_vreg.end()) {
                vregs.insert(vregs.end(), it->second.begin(), it->second.end());
            }
        }
    }    
    return result;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::integer_constant& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    return linear::literal(std::bit_cast<uint64_t>(node.value()), calculator(*node.get_type().type()).size);
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::floating_constant& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    return linear::literal(std::bit_cast<uint64_t>(node.value()), calculator(*node.get_type().type()).size);
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::string_constant& node) {
    std::ostringstream ss;
    ss << "string_" << node.index();
    
    auto dest_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::load_effective_address>(dest_reg, ss.str()));
    return dest_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::variable_reference& node) {
    auto variable = node.get_variable();

    std::vector<linear::var_info> vregs = m_lowerer.m_current_block->var_info.m_variable_to_vreg.at(variable);
    
    // if only one definition we are gtg
    if (vregs.size() == 1) {
        if (vregs.at(0).block_id != m_lowerer.current_block_id()) {
            m_lowerer.m_current_block->incoming_vregs.push_back(vregs.at(0));
        }
        return vregs.at(0).vreg;
    }

    // if multiple definitions we need to reconcile with phi node
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    auto dest_reg = m_lowerer.new_vreg(calculator(*variable->get_type().type()).size * 8);
    
    m_lowerer.m_current_block->incoming_vregs.insert(m_lowerer.m_current_block->incoming_vregs.end(), vregs.begin(), vregs.end());
    m_lowerer.emit(std::make_unique<linear::phi_instruction>(dest_reg, std::move(vregs)));

    // update the current block's var info
    m_lowerer.m_current_block->var_info.m_variable_to_vreg[variable] = { linear::var_info{ .vreg = dest_reg, .block_id = m_lowerer.current_block_id() } };

    return dest_reg;
}