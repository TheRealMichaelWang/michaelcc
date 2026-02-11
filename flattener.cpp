#include "linear/flatten.hpp"
#include "linear/ir.hpp"
#include "logic/ir.hpp"
#include "logic/type_info.hpp"
#include <bit>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <variant>
#include <stdexcept>

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

linear::virtual_register logic_lowerer::get_var_reg(const std::shared_ptr<logic::variable>& variable) {
    std::vector<linear::var_info> vregs = m_current_block->var_info.m_variable_to_vreg.at(variable);
    
    // if only one definition we are gtg
    if (vregs.size() == 1) {
        if (vregs.at(0).block_id != current_block_id()) {
            m_current_block->incoming_vregs.push_back(vregs.at(0));
        }
        return vregs.at(0).vreg;
    }

    // if multiple definitions we need to reconcile with phi node
    type_layout_calculator calculator(m_platform_info);
    auto dest_reg = new_vreg(calculator(*variable->get_type().type()).size * 8);
    
    m_current_block->incoming_vregs.insert(m_current_block->incoming_vregs.end(), vregs.begin(), vregs.end());
    emit(std::make_unique<linear::phi_instruction>(dest_reg, std::move(vregs)));

    // update the current block's var info
    m_current_block->var_info.m_variable_to_vreg[variable] = { linear::var_info{ .vreg = dest_reg, .block_id = current_block_id() } };

    return dest_reg;
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
    ss << "@string_" << node.index();
    
    auto dest_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::load_effective_address>(dest_reg, ss.str()));
    return dest_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::variable_reference& node) {
    auto variable = node.get_variable();
    auto var_reg = m_lowerer.get_var_reg(variable);
    if (variable->must_alloca()) {
        type_layout_calculator calculator(m_lowerer.m_platform_info);
        auto dest_reg = m_lowerer.new_vreg(calculator(*variable->get_type().type()).size * 8);

        m_lowerer.emit(std::make_unique<linear::m_instruction>(
            linear::m_instruction_type::LOAD, 
            dest_reg, 
            var_reg, 
            0
        ));

        return dest_reg;
    }
    return var_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::function_reference& node) {
    auto dest_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::load_effective_address>(dest_reg, node.get_function()->name()));
    return dest_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::increment_operator& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    size_t var_size = calculator(*node.get_type().type()).size * 8;

    bool use_one = node.get_operator() == MICHAELCC_TOKEN_INCREMENT || node.get_operator() == MICHAELCC_TOKEN_DECREMENT;
    
    if (std::holds_alternative<std::shared_ptr<logic::variable>>(node.destination())) {
        auto variable = std::get<std::shared_ptr<logic::variable>>(node.destination());
        if (!variable->must_alloca()) {
            auto dest_reg = m_lowerer.get_var_reg(variable);

            linear::a_instruction_type type;
            switch (node.get_operator()) {
            case MICHAELCC_TOKEN_INCREMENT:
            case MICHAELCC_TOKEN_INCREMENT_BY:
                type = linear::a_instruction_type::ADD;
                break;
            case MICHAELCC_TOKEN_DECREMENT:
            case MICHAELCC_TOKEN_DECREMENT_BY:
                type = linear::a_instruction_type::SUBTRACT;
                break;
            default:
                throw std::runtime_error("Invalid increment operator");
            }

            auto new_dest_reg = m_lowerer.new_vreg(var_size);
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                type, 
                new_dest_reg, 
                dest_reg, 
                use_one ? linear::literal{ .value = 1, .size_bits = var_size } 
                    : m_lowerer.lower_expression(*node.increment_amount().value())
            ));

            
            m_lowerer.m_current_block->var_info.m_variable_to_vreg[variable] = { linear::var_info{ .vreg = new_dest_reg, .block_id = m_lowerer.current_block_id() } };
            return new_dest_reg;
        }
    }

    auto dest_reg = std::visit(overloaded{
        [this](const std::unique_ptr<logic::expression>& expr) -> linear::virtual_register {
            return m_lowerer.compute_lvalue_address(*expr);
        },
        [this](const std::shared_ptr<logic::variable>& variable) -> linear::virtual_register {
            return m_lowerer.get_var_reg(variable);
        }
    }, node.destination());

    linear::i_instruction_type type;
    switch (node.get_operator()) {
    case MICHAELCC_TOKEN_INCREMENT:
    case MICHAELCC_TOKEN_INCREMENT_BY:
        type = linear::i_instruction_type::INCREMENT;
        break;
    case MICHAELCC_TOKEN_DECREMENT:
    case MICHAELCC_TOKEN_DECREMENT_BY:
        type = linear::i_instruction_type::DECREMENT;
        break;
    default:
        throw std::runtime_error("Invalid increment operator");
    }

    m_lowerer.emit(std::make_unique<linear::i_instruction>(
        type, dest_reg, 
        m_lowerer.lower_expression(*node.increment_amount().value()))
    );

    auto result_reg = m_lowerer.new_vreg(var_size);
    m_lowerer.emit(std::make_unique<linear::m_instruction>(
        linear::m_instruction_type::LOAD, 
        result_reg, 
        dest_reg, 
        0
    ));
    return result_reg;
}