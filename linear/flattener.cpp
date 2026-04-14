#include "linear/registers.hpp"
#include "linear/flatten.hpp"
#include "linear/ir.hpp"
#include "linear/dominators.hpp"
#include "linear/registers.hpp"
#include "logic/ir.hpp"
#include "logic/type_info.hpp"
#include "logic/typing.hpp"
#include "linear/static.hpp"
#include <algorithm>
#include <format>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <variant>
#include <stdexcept>
#include <cassert>
#include <utility>
#include <string>

using namespace michaelcc;

logic_lowerer::block_var_ctx logic_lowerer::reconcile_var_regs(const std::vector<size_t>& incoming_block_ids) {
    block_var_ctx result;

    // search and find all variables that are defined in any of the incoming blocks
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
                vregs.push_back(linear::var_info{ .vreg = it->second.vreg, .block_id = block_id });
            }
        }
        if (vregs.size() == 1) {
            result.m_variable_to_vreg[variable] = vregs.at(0);
        }
        else {
            type_layout_calculator calculator(get_platform_info());
            auto layout = calculator(*variable->get_type().type());
            auto dest_reg = m_translation_unit.new_vreg(
                type_layout_info::get_register_size(layout.size), 
                get_register_class(variable->get_type())
            );
            if (variable->must_use_register()) {
                m_translation_unit.cannot_spill_vregs.insert(dest_reg);
            }

            emit(std::make_unique<linear::phi_instruction>(dest_reg, std::vector<linear::var_info>(vregs)));

            result.m_variable_to_vreg[variable] = linear::var_info{ .vreg = dest_reg, .block_id = current_block_id() };
        }
    }    
    return result;
}

void logic_lowerer::emit_phi_all() {
    std::unordered_map<std::shared_ptr<logic::variable>, linear::phi_instruction*> init_phi_nodes;
    for (const auto& [variable, var_info] : m_current_block->var_info.m_variable_to_vreg) {
        type_layout_calculator calculator(get_platform_info());
        auto var_layout = calculator(*variable->get_type().type());
        auto dest_reg = m_translation_unit.new_vreg(
            type_layout_info::get_register_size(var_layout.size),
            get_register_class(variable->get_type())
        );
        if (variable->must_use_register()) {
            m_translation_unit.cannot_spill_vregs.insert(dest_reg);
        }
        auto phi_node = std::make_unique<linear::phi_instruction>(dest_reg, std::vector<linear::var_info>({ var_info }));
        init_phi_nodes[variable] = phi_node.get();
        emit(std::move(phi_node));
        m_current_block->var_info.m_variable_to_vreg[variable] = linear::var_info{ .vreg = dest_reg, .block_id = current_block_id() };
    }
    m_loop_infos[current_block_id()] = loop_info{ 
        .block_id = current_block_id(), 
        .finish_block_id = allocate_block_id(),
        .init_phi_nodes = std::move(init_phi_nodes)
    };
    m_loop_stack.push_back(current_block_id());
}

void logic_lowerer::recurse_block(size_t source_block_id, size_t target_block_id) {
    auto& source_block_var_ctx = m_finished_block_var_ctx.at(source_block_id);
    auto& target_loop_info = m_loop_infos.at(target_block_id);

    for (const auto& [variable, var_info] : source_block_var_ctx.m_variable_to_vreg) {
        auto it = target_loop_info.init_phi_nodes.find(variable);
        if (it != target_loop_info.init_phi_nodes.end()) {
            it->second->augment_value(var_info);
        }
    }
}

linear::virtual_register logic_lowerer::get_var_reg(const std::shared_ptr<logic::variable>& variable) {
    if (variable->use_static_storage()) {
        auto addr_reg = m_translation_unit.new_vreg(
            get_platform_info().pointer_size,
            linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
        );
        std::string label = m_current_function.has_value() ? m_current_function->static_var_labels.at(variable) : variable->name();
        emit(std::make_unique<linear::load_effective_address>(addr_reg, label));
        return addr_reg;
    }
    
    auto var_reg_it = m_current_block->var_info.m_variable_to_vreg.find(variable);
    if (var_reg_it != m_current_block->var_info.m_variable_to_vreg.end()) {
        return var_reg_it->second.vreg;
    }

    auto parameter_it = m_current_function->parameters.find(variable->name());
    if (parameter_it != m_current_function->parameters.end()) {
        if (parameter_it->second.pass_via_stack) {
            auto var_addr_reg = m_translation_unit.new_vreg(
                get_platform_info().pointer_size,
                linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
            );
            emit(std::make_unique<linear::load_parameter>(
                var_addr_reg,
                parameter_it->second
            ));
            return var_addr_reg;
        }
        else {
            auto var_reg = m_translation_unit.new_vreg(
                type_layout_info::get_register_size(parameter_it->second.layout.size),
                parameter_it->second.register_class
            );
            emit(std::make_unique<linear::load_parameter>(var_reg, parameter_it->second));
            return var_reg;
        }
    }

    throw std::runtime_error(std::format("Variable \"{}\" not found in current block or parameters", variable->name()));
}

void logic_lowerer::emit_iloop(linear::virtual_register count, std::function<void(linear::virtual_register)> body) {
    auto initial_state = m_translation_unit.new_vreg(get_platform_info().int_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
    emit(std::make_unique<linear::init_register>(initial_state, linear::register_word{ .uint64 = 0 }));

    size_t old_block_id = current_block_id();
    size_t loop1_block_id = allocate_block_id();
    size_t loop2_block_id = allocate_block_id();
    size_t finish_block_id = allocate_block_id();

    emit(std::make_unique<linear::branch>(loop1_block_id));
    seal_block(); //end current block

    begin_block(loop1_block_id, { old_block_id });
    auto iterator_state = m_translation_unit.new_vreg(get_platform_info().int_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
    auto post_increment_state = m_translation_unit.new_vreg(get_platform_info().int_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);

    emit(std::make_unique<linear::phi_instruction>(
        iterator_state, std::vector<linear::var_info>{ 
            linear::var_info{ .vreg = initial_state, .block_id = old_block_id },
            linear::var_info{ .vreg = post_increment_state, .block_id = loop2_block_id }
        }
    ));

    auto cond_check_status = m_translation_unit.new_vreg(get_platform_info().int_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
    emit(std::make_unique<linear::a_instruction>(
        linear::MICHAELCC_LINEAR_A_COMPARE_EQUAL, cond_check_status, iterator_state, count
    ));

    emit(std::make_unique<linear::branch_condition>(
        cond_check_status, finish_block_id, loop2_block_id, true
    ));
    seal_block();

    begin_block(loop2_block_id, { loop1_block_id });

    body(iterator_state);

    //increment and branch back to the top
    emit(std::make_unique<linear::a2_instruction>(
        linear::MICHAELCC_LINEAR_A_ADD, post_increment_state, iterator_state, 1
    ));
    emit(std::make_unique<linear::branch>(loop1_block_id));
    seal_block();

    begin_block(finish_block_id, { loop1_block_id });
}

void logic_lowerer::emit_memset(linear::virtual_register dest, linear::virtual_register value, linear::virtual_register count) {
    emit_iloop(count, [this, dest, value](linear::virtual_register iterator_state) {
        auto offset_reg = m_translation_unit.new_vreg(get_platform_info().int_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
        emit(std::make_unique<linear::a2_instruction>(
            linear::MICHAELCC_LINEAR_A_MULTIPLY, 
            offset_reg, 
            iterator_state, 
            static_cast<size_t>(value.reg_size) / 8
        ));
        auto address_reg = m_translation_unit.new_vreg(get_platform_info().pointer_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
        emit(std::make_unique<linear::a_instruction>(
            linear::MICHAELCC_LINEAR_A_ADD, address_reg, dest, offset_reg
        ));
        emit(std::make_unique<linear::store_memory>(address_reg, value, 0));
    });
}

void logic_lowerer::emit_memcpy(linear::virtual_register dest, linear::virtual_register src, size_t size_bytes, size_t offset) {
    for (size_t i = 0; i < size_bytes; i += 1) {
        auto elem_reg = m_translation_unit.new_vreg(
            linear::word_size::MICHAELCC_WORD_SIZE_BYTE, 
            linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
        );    
        emit(std::make_unique<linear::load_memory>(
            elem_reg, 
            src, 
            i + offset
        ));
        emit(std::make_unique<linear::store_memory>(
            dest, 
            elem_reg, 
            i + offset
        ));
    }
}

void logic_lowerer::lower_struct_initializer(const logic::struct_initializer& node, linear::virtual_register dest_address, size_t offset) {
    auto& struct_type = node.struct_type();
    
    type_layout_calculator calculator(get_platform_info());
    for (const auto& initializer : node.initializers()) {
        auto field = std::find_if(struct_type->fields().begin(), struct_type->fields().end(), [&](const typing::member& member) {
            return member.name == initializer.member_name;
        });
        if (field == struct_type->fields().end()) {
            throw std::runtime_error("Member \"" + initializer.member_name + "\" not found in struct.");
        }

        lower_at_address(dest_address, initializer.initializer, field->offset + offset);
    }
}

void logic_lowerer::lower_union_initializer(const logic::union_initializer& node, linear::virtual_register dest_address, size_t offset) {
    type_layout_calculator calculator(get_platform_info());
    assert(node.target_member().offset == 0);
    assert(calculator.must_alloca(node.union_type()));

    lower_at_address(dest_address, node.initializer(), 0);
}

std::optional<size_t> logic_lowerer::lower_statements(const std::vector<std::unique_ptr<logic::statement>>& statements) {
    size_t last_block_id = current_block_id();
    for (const auto& statement : statements) {
        assert(m_current_block.has_value());
        lower_statement(*statement);

        if (m_current_block.has_value()) {
            last_block_id = current_block_id();
        }
        else {
            return std::nullopt;
        }
    }
    return last_block_id;
}

linear::virtual_register logic_lowerer::lower_at_address(linear::virtual_register dest_address, const std::unique_ptr<logic::expression>& initializer, size_t offset) {
    type_layout_calculator calculator(get_platform_info());
    auto layout = calculator(*initializer->get_type().type());
    if(calculator.must_alloca(initializer->get_type().type())) {
        if (auto struct_initializer = dynamic_cast<logic::struct_initializer*>(initializer.get())) {
            lower_struct_initializer(*struct_initializer, dest_address, offset);
        }
        else if (auto union_initializer = dynamic_cast<logic::union_initializer*>(initializer.get())) {
            lower_union_initializer(*union_initializer, dest_address, offset);
        }
        else {
            auto src_addr = lower_expression(*initializer);
            emit_memcpy(
                dest_address, 
                src_addr, 
                layout.size, 
                offset
            );
        }
        return dest_address;
    }
    else {
        auto evaled_src_reg = lower_expression(*initializer);
        assert(static_cast<size_t>(evaled_src_reg.reg_size) / 8 == layout.size);
        emit(std::make_unique<linear::store_memory>(
            dest_address, 
            evaled_src_reg, 
            offset
        ));
        return evaled_src_reg;
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::integer_constant& node) {
    auto [value, reg_size] = linear::static_storage::int_literal_to_regword(node, m_lowerer.get_platform_info());
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(reg_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
    m_lowerer.emit(std::make_unique<linear::init_register>(
        dest_reg, value
    ));

    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::floating_constant& node) {
    std::shared_ptr<typing::float_type> float_type = std::static_pointer_cast<typing::float_type>(node.get_type().type());
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
        type_layout_info::get_register_size(float_type->type_class() == typing::FLOAT_FLOAT_CLASS 
            ? static_cast<size_t>(linear::word_size::MICHAELCC_WORD_SIZE_UINT32) / 8
            : static_cast<size_t>(linear::word_size::MICHAELCC_WORD_SIZE_UINT64) / 8
        ), linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT
    );

    linear::register_word value;
    if (float_type->type_class() == typing::DOUBLE_FLOAT_CLASS) {
        value.float64 = node.value();
    }
    else {
        assert(float_type->type_class() == typing::FLOAT_FLOAT_CLASS);
        value.float32 = node.value();
    }

    m_lowerer.emit(std::make_unique<linear::init_register>(
        dest_reg, value
    ));

    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::string_constant& node) {
    std::ostringstream ss;
    ss << "@string_" << node.index();
    
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(m_lowerer.get_platform_info().pointer_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
    m_lowerer.emit(std::make_unique<linear::load_effective_address>(
        dest_reg, ss.str()
    ));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::enumerator_literal& node) {
    int64_t enumerator_value = node.enumerator().value;

    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(m_lowerer.get_platform_info().int_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);

    linear::register_word value = linear::static_storage::const_to_regword(enumerator_value, m_lowerer.get_platform_info().int_size, true);

    m_lowerer.emit(std::make_unique<linear::init_register>(
        dest_reg, value
    ));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::variable_reference& node) {
    auto variable = node.get_variable();
    auto var_reg = m_lowerer.get_var_reg(variable);
    type_layout_calculator calculator(m_lowerer.get_platform_info());

    if (variable->must_alloca() || calculator.must_alloca(variable->get_type())) {
        if (!calculator.must_alloca(variable->get_type())) {
            auto layout = calculator(*variable->get_type().type());
            auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
                type_layout_info::get_register_size(layout.size),
                m_lowerer.get_register_class(variable->get_type())
            );
            assert(static_cast<size_t>(var_reg.reg_size) / 8 == layout.size);
            m_lowerer.emit(std::make_unique<linear::load_memory>(
                dest_reg, 
                var_reg, 
                0
            ));

            return dest_reg;
        }
    }
    return var_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::set_variable& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto var_reg = m_lowerer.get_var_reg(node.variable());
    
    if (node.variable()->must_alloca() || calculator.must_alloca(node.variable()->get_type())) {
        return m_lowerer.lower_at_address(var_reg, node.value(), 0);
    }

    auto new_reg = m_lowerer.lower_expression(
        *node.value()
    );
    
    if (m_lowerer.m_translation_unit.vreg_colors.contains(var_reg)) {
        m_lowerer.m_translation_unit.vreg_colors[new_reg] = m_lowerer.m_translation_unit.vreg_colors.at(var_reg);
    }
    if (m_lowerer.m_translation_unit.cannot_spill_vregs.contains(var_reg)) {
        m_lowerer.m_translation_unit.cannot_spill_vregs.insert(new_reg);
    }

    assert(new_reg != var_reg);

    m_lowerer.m_current_block->var_info.m_variable_to_vreg[node.variable()] = linear::var_info{ 
        .vreg = new_reg, 
        .block_id = m_lowerer.current_block_id() 
    };

    return new_reg;
}

void logic_lowerer::statement_lowerer::dispatch(const logic::variable_declaration& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto var_layout = calculator(*node.variable()->get_type().type());

    if (node.variable()->use_static_storage()) {
        m_lowerer.lower_static_variable_declaration(node);
    }
    else if (node.variable()->must_alloca() || calculator.must_alloca(node.variable()->get_type())) {
        auto var_reg = m_lowerer.m_translation_unit.new_vreg(
            m_lowerer.get_platform_info().pointer_size,
            linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
        );
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(
            var_reg, 
            var_layout.size, 
            var_layout.alignment
        ));

        m_lowerer.lower_at_address(var_reg, node.initializer(), 0);
        
        if (node.variable()->must_use_register()) {
            m_lowerer.m_translation_unit.cannot_spill_vregs.insert(var_reg);
        }
        m_lowerer.m_current_block->var_info.m_variable_to_vreg[node.variable()] = linear::var_info{ 
            .vreg = var_reg, 
            .block_id = m_lowerer.current_block_id() 
        };
    }
    else {
        auto var_reg = m_lowerer.lower_expression(*node.initializer());
        
        if (node.variable()->must_use_register()) {
            m_lowerer.m_translation_unit.cannot_spill_vregs.insert(var_reg);
        }
        m_lowerer.m_current_block->var_info.m_variable_to_vreg[node.variable()] = linear::var_info{ 
            .vreg = var_reg, 
            .block_id = m_lowerer.current_block_id() 
        };
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::set_address& node) {
    auto dest_addr = m_lowerer.lower_expression(*node.destination());
    return m_lowerer.lower_at_address(dest_addr, node.value(), 0);
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::function_reference& node) {
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(m_lowerer.get_platform_info().pointer_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
    m_lowerer.emit(std::make_unique<linear::load_effective_address>(dest_reg, node.get_function()->name()));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::increment_operator& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto layout = calculator(*node.get_type().type());
    
    bool use_one = node.get_operator() == MICHAELCC_TOKEN_INCREMENT || node.get_operator() == MICHAELCC_TOKEN_DECREMENT;
    linear::a_instruction_type type;
    switch (node.get_operator()) {
    case MICHAELCC_TOKEN_INCREMENT:
    case MICHAELCC_TOKEN_INCREMENT_BY:
        if (node.get_type().is_same_type<typing::float_type>()) { type = linear::a_instruction_type::MICHAELCC_LINEAR_A_FLOAT_ADD; } else { type = linear::a_instruction_type::MICHAELCC_LINEAR_A_ADD; }
        break;
    case MICHAELCC_TOKEN_DECREMENT:
    case MICHAELCC_TOKEN_DECREMENT_BY:
        if (node.get_type().is_same_type<typing::float_type>()) { type = linear::a_instruction_type::MICHAELCC_LINEAR_A_FLOAT_SUBTRACT; } else { type = linear::a_instruction_type::MICHAELCC_LINEAR_A_SUBTRACT; }
        break;
    default:
        throw std::runtime_error("Invalid increment operator");
    }

    if (std::holds_alternative<std::shared_ptr<logic::variable>>(node.destination())) {
        auto variable = std::get<std::shared_ptr<logic::variable>>(node.destination());
        if (!variable->must_alloca()) {
            auto var_reg = m_lowerer.get_var_reg(variable);
            auto new_var_reg = m_lowerer.m_translation_unit.new_vreg(
                type_layout_info::get_register_size(layout.size),
                m_lowerer.get_register_class(node.get_type().type())
            );

            if (use_one) {
                m_lowerer.emit(std::make_unique<linear::a2_instruction>(
                    type, 
                    new_var_reg, 
                    var_reg, 
                    1
                ));
            }
            else {
                m_lowerer.emit(std::make_unique<linear::a_instruction>(
                    type, 
                    new_var_reg, 
                    var_reg, 
                    m_lowerer.lower_expression(*node.increment_amount().value())
                ));
            }
            
            m_lowerer.m_current_block->var_info.m_variable_to_vreg[variable] = linear::var_info{ .vreg = new_var_reg, .block_id = m_lowerer.current_block_id() };
            return var_reg;
        }
    }

    assert(!calculator.must_alloca(node.get_type().type()));

    auto value_addr = std::visit(overloaded{
        [this](const std::unique_ptr<logic::expression>& expr) -> linear::virtual_register {
            return m_lowerer.compute_lvalue_address(*expr);
        },
        [this](const std::shared_ptr<logic::variable>& variable) -> linear::virtual_register {
            return m_lowerer.get_var_reg(variable);
        }
    }, node.destination());

    auto current_value_reg = m_lowerer.m_translation_unit.new_vreg(
        type_layout_info::get_register_size(layout.size),
        m_lowerer.get_register_class(node.get_type().type())
    );
    m_lowerer.emit(std::make_unique<linear::load_memory>(
        current_value_reg, 
        value_addr, 
        0
    ));
    auto incremented_reg = m_lowerer.m_translation_unit.new_vreg(
        type_layout_info::get_register_size(layout.size),
        m_lowerer.get_register_class(node.get_type().type())
    );
    if (use_one) {
        m_lowerer.emit(std::make_unique<linear::a2_instruction>(
            type, 
            incremented_reg, 
            current_value_reg, 
            1
        ));
    }
    else {
        m_lowerer.emit(std::make_unique<linear::a_instruction>(
            type, 
            incremented_reg, 
            current_value_reg, 
            m_lowerer.lower_expression(*node.increment_amount().value())
        ));
    }

    m_lowerer.emit(std::make_unique<linear::store_memory>(
        value_addr, 
        incremented_reg, 
        0
    ));
    return current_value_reg;
}

static linear::a_instruction_type token_to_a_type(token_type op, bool is_float, bool is_signed) {
    switch (op) {
        case MICHAELCC_TOKEN_PLUS:           return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_ADD : linear::MICHAELCC_LINEAR_A_ADD;
        case MICHAELCC_TOKEN_MINUS:          return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_SUBTRACT : linear::MICHAELCC_LINEAR_A_SUBTRACT;
        case MICHAELCC_TOKEN_ASTERISK:       return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_MULTIPLY : linear::MICHAELCC_LINEAR_A_MULTIPLY;
        case MICHAELCC_TOKEN_SLASH:          return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_DIVIDE : is_signed ? linear::MICHAELCC_LINEAR_A_SIGNED_DIVIDE : linear::MICHAELCC_LINEAR_A_UNSIGNED_DIVIDE;
        case MICHAELCC_TOKEN_MODULO:         return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_MODULO : is_signed ? linear::MICHAELCC_LINEAR_A_SIGNED_MODULO : linear::MICHAELCC_LINEAR_A_UNSIGNED_MODULO;
        case MICHAELCC_TOKEN_BITSHIFT_LEFT:  return linear::MICHAELCC_LINEAR_A_SHIFT_LEFT;
        case MICHAELCC_TOKEN_BITSHIFT_RIGHT: return is_signed ? linear::MICHAELCC_LINEAR_A_SIGNED_SHIFT_RIGHT : linear::MICHAELCC_LINEAR_A_UNSIGNED_SHIFT_RIGHT;
        case MICHAELCC_TOKEN_AND:            return linear::MICHAELCC_LINEAR_A_BITWISE_AND;
        case MICHAELCC_TOKEN_OR:             return linear::MICHAELCC_LINEAR_A_BITWISE_OR;
        case MICHAELCC_TOKEN_CARET:          return linear::MICHAELCC_LINEAR_A_BITWISE_XOR;
        case MICHAELCC_TOKEN_DOUBLE_AND:     return linear::MICHAELCC_LINEAR_A_AND;
        case MICHAELCC_TOKEN_DOUBLE_OR:      return linear::MICHAELCC_LINEAR_A_OR;
        case MICHAELCC_TOKEN_EQUALS:         return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_EQUAL : linear::MICHAELCC_LINEAR_A_COMPARE_EQUAL;
        case MICHAELCC_TOKEN_NOT_EQUALS:     return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_NOT_EQUAL : linear::MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL;
        case MICHAELCC_TOKEN_LESS:           return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN : is_signed ? linear::MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN : linear::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN;
        case MICHAELCC_TOKEN_LESS_EQUAL:     return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN_OR_EQUAL : is_signed ? linear::MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN_OR_EQUAL : linear::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN_OR_EQUAL;
        case MICHAELCC_TOKEN_MORE:           return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN : is_signed ? linear::MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN : linear::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN;
        case MICHAELCC_TOKEN_MORE_EQUAL:     return is_float ? linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN_OR_EQUAL : is_signed ? linear::MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN_OR_EQUAL : linear::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN_OR_EQUAL;
        default: throw std::runtime_error("Unsupported binary operator for a_instruction");
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::arithmetic_operator& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto result_layout = calculator(*node.get_type().type());

    linear::virtual_register left = m_lowerer.lower_expression(*node.left());
    linear::virtual_register right = m_lowerer.lower_expression(*node.right());

    assert(node.left()->get_type().type()->is_equivalent_to(*node.right()->get_type().type(), m_lowerer.get_platform_info()));
    bool is_float = node.left()->get_type().is_same_type<typing::float_type>();
    bool is_signed = node.left()->get_type().is_same_type<typing::int_type>() && (std::dynamic_pointer_cast<const typing::int_type>(node.left()->get_type().type())->is_signed());

    linear::a_instruction_type type = token_to_a_type(node.get_operator(), is_float, is_signed);
    auto result_reg = m_lowerer.m_translation_unit.new_vreg(
        type_layout_info::get_register_size(result_layout.size),
        m_lowerer.get_register_class(node.get_type().type())
    );
    m_lowerer.emit(std::make_unique<linear::a_instruction>(type, result_reg, left, right));
    return result_reg;
}

static linear::u_instruction_type token_to_u_type(token_type op, bool is_float) {
    switch (op) {
        case MICHAELCC_TOKEN_MINUS: return is_float ? linear::MICHAELCC_LINEAR_U_FLOAT_NEGATE : linear::MICHAELCC_LINEAR_U_NEGATE;
        case MICHAELCC_TOKEN_NOT: return linear::MICHAELCC_LINEAR_U_NOT;
        case MICHAELCC_TOKEN_TILDE: return linear::MICHAELCC_LINEAR_U_BITWISE_NOT;
        default: throw std::runtime_error("Unsupported unary operator");
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::unary_operation& node) {
    auto operand = m_lowerer.lower_expression(*node.operand());
    type_layout_calculator calculator(m_lowerer.get_platform_info());

    auto result_layout = calculator(*node.get_type().type());
    auto dest = m_lowerer.m_translation_unit.new_vreg(
        type_layout_info::get_register_size(result_layout.size), 
        m_lowerer.get_register_class(node.get_type())
    );

    auto u_operator = token_to_u_type(node.get_operator(), node.get_type().is_same_type<typing::float_type>());
    m_lowerer.emit(std::make_unique<linear::u_instruction>(u_operator, dest, operand));
    return dest;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::type_cast& node) {
    auto operand = m_lowerer.lower_expression(*node.operand());

    if (node.get_type().type()->is_equivalent_to(*node.operand()->get_type().type(), m_lowerer.get_platform_info())) {
        return operand;
    }

    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto target_layout = calculator(*node.get_type().type());
    auto target_word_size = type_layout_info::get_register_size(target_layout.size);
    if (node.get_type().is_same_type<typing::float_type>() && node.operand()->get_type().is_same_type<typing::int_type>()) {
        std::shared_ptr<typing::int_type> operand_int_type = std::dynamic_pointer_cast<typing::int_type>(node.operand()->get_type().type());
        auto adjusted_operand = operand;
        if (type_layout_calculator::get_int_type_size(*operand_int_type, m_lowerer.get_platform_info()) != target_word_size) {
            adjusted_operand = m_lowerer.m_translation_unit.new_vreg(
                target_word_size,
                linear::MICHAELCC_REGISTER_CLASS_INTEGER
            );
            if (operand_int_type->is_signed()) {
                m_lowerer.emit(std::make_unique<linear::c_instruction>(
                    linear::MICHAELCC_LINEAR_C_SEXT_OR_TRUNC,
                    adjusted_operand,
                    operand
                ));
            }
            else {
                m_lowerer.emit(std::make_unique<linear::c_instruction>(
                    linear::MICHAELCC_LINEAR_C_ZEXT_OR_TRUNC,
                    adjusted_operand,
                    operand
                ));
            }
        }

        if (target_word_size == linear::MICHAELCC_WORD_SIZE_UINT64) {
            auto dest = m_lowerer.m_translation_unit.new_vreg(
                linear::MICHAELCC_WORD_SIZE_UINT64,
                linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                operand_int_type->is_signed() ? linear::MICHAELCC_LINEAR_C_SIGNED_INT64_TO_FLOAT64 : linear::MICHAELCC_LINEAR_C_UNSIGNED_INT64_TO_FLOAT64, 
                dest, 
                adjusted_operand
            ));
            return dest;
        }
        else if (target_word_size == linear::MICHAELCC_WORD_SIZE_UINT32) {
            auto dest = m_lowerer.m_translation_unit.new_vreg(
                linear::MICHAELCC_WORD_SIZE_UINT32,
                linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                operand_int_type->is_signed() ? linear::MICHAELCC_LINEAR_C_SIGNED_INT32_TO_FLOAT32 : linear::MICHAELCC_LINEAR_C_UNSIGNED_INT32_TO_FLOAT32, 
                dest, 
                adjusted_operand
            ));
            return dest;
        }
        else {
            throw std::runtime_error(std::format("Invalid bitwidth {} for int to float type cast.", static_cast<int>(operand.reg_size)));
        }
    }
    else if (node.get_type().is_same_type<typing::int_type>() && node.operand()->get_type().is_same_type<typing::float_type>()) {
        std::shared_ptr<typing::int_type> target_int_type = std::dynamic_pointer_cast<typing::int_type>(node.get_type().type());
        linear::virtual_register dest;
        if (operand.reg_size == linear::MICHAELCC_WORD_SIZE_UINT64) {
            dest = m_lowerer.m_translation_unit.new_vreg(
                linear::MICHAELCC_WORD_SIZE_UINT64,
                linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                target_int_type->is_signed() ? linear::MICHAELCC_LINEAR_C_FLOAT64_TO_SIGNED_INT64 : linear::MICHAELCC_LINEAR_C_FLOAT64_TO_UNSIGNED_INT64, 
                dest, 
                operand
            ));
        }
        else if (operand.reg_size == linear::MICHAELCC_WORD_SIZE_UINT32) {
            dest = m_lowerer.m_translation_unit.new_vreg(
                linear::MICHAELCC_WORD_SIZE_UINT32,
                linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                target_int_type->is_signed() ? linear::MICHAELCC_LINEAR_C_FLOAT32_TO_SIGNED_INT32 : linear::MICHAELCC_LINEAR_C_FLOAT32_TO_UNSIGNED_INT32, 
                dest, 
                operand
            ));
        }
        else {
            throw std::runtime_error(std::format("Invalid bitwidth {} for float to int type cast.", static_cast<int>(operand.reg_size)));
        }

        if (dest.reg_size != type_layout_calculator::get_int_type_size(*target_int_type, m_lowerer.get_platform_info())) {
            auto new_vreg = m_lowerer.m_translation_unit.new_vreg(
                type_layout_calculator::get_int_type_size(*target_int_type, m_lowerer.get_platform_info()),
                linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                target_int_type->is_signed() ? linear::MICHAELCC_LINEAR_C_SEXT_OR_TRUNC : linear::MICHAELCC_LINEAR_C_ZEXT_OR_TRUNC,
                new_vreg, dest
            ));
            return new_vreg;
        }
        return dest;
    }
    else if (node.get_type().is_same_type<typing::float_type>() && node.operand()->get_type().is_same_type<typing::float_type>()) {
        if (operand.reg_size == linear::MICHAELCC_WORD_SIZE_UINT64 && target_word_size == linear::MICHAELCC_WORD_SIZE_UINT32) {
            auto dest = m_lowerer.m_translation_unit.new_vreg(
                linear::MICHAELCC_WORD_SIZE_UINT32,
                linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                linear::MICHAELCC_LINEAR_C_FLOAT64_TO_FLOAT32, 
                dest, 
                operand
            ));
            return dest;
        }
        else if (operand.reg_size == linear::MICHAELCC_WORD_SIZE_UINT32 && target_word_size == linear::MICHAELCC_WORD_SIZE_UINT64) {
            auto dest = m_lowerer.m_translation_unit.new_vreg(
                linear::MICHAELCC_WORD_SIZE_UINT64,
                linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                linear::MICHAELCC_LINEAR_C_FLOAT32_TO_FLOAT64, 
                dest, 
                operand
            ));
            return dest;
        }
        else {
            throw std::runtime_error(std::format("Invalid bitwidth {} for float to float type cast.", static_cast<int>(operand.reg_size)));
        }
    }
    else if (node.get_type().is_same_type<typing::int_type>() && node.operand()->get_type().is_same_type<typing::int_type>()) {
        std::shared_ptr<typing::int_type> target_int_type = std::dynamic_pointer_cast<typing::int_type>(node.get_type().type());
        std::shared_ptr<typing::int_type> operand_int_type = std::dynamic_pointer_cast<typing::int_type>(node.operand()->get_type().type());
        
        if (target_int_type->is_signed() && operand_int_type->is_signed()) { //sign extension or truncation
            if (target_word_size == operand.reg_size) {
                return operand;
            }
            
            auto new_vreg = m_lowerer.m_translation_unit.new_vreg(
                target_word_size,
                m_lowerer.get_register_class(node.get_type())
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                linear::MICHAELCC_LINEAR_C_SEXT_OR_TRUNC,
                new_vreg,
                operand
            ));
            return new_vreg;
        }
        else {
            if (target_word_size == operand.reg_size) {
                return operand;
            }

            auto new_vreg = m_lowerer.m_translation_unit.new_vreg(
                target_word_size,
                m_lowerer.get_register_class(node.get_type())
            );
            m_lowerer.emit(std::make_unique<linear::c_instruction>(
                linear::MICHAELCC_LINEAR_C_ZEXT_OR_TRUNC,
                new_vreg,
                operand
            ));
            return new_vreg;
        }
    }
    else if (node.get_type().is_same_type<typing::pointer_type>() && (node.operand()->get_type().is_same_type<typing::pointer_type>() || node.operand()->get_type().is_same_type<typing::function_pointer_type>())) {
        return operand; //allow passthrough
    }
    else if (node.get_type().is_same_type<typing::function_pointer_type>() && (node.operand()->get_type().is_same_type<typing::function_pointer_type>() || node.operand()->get_type().is_same_type<typing::pointer_type>())) {
        return operand; //allow passthrough
    }
    else if (node.get_type().is_same_type<typing::pointer_type>() && node.operand()->get_type().is_same_type<typing::array_type>()) {
        std::shared_ptr<typing::array_type> operand_array_type = std::dynamic_pointer_cast<typing::array_type>(node.operand()->get_type().type());
        std::shared_ptr<typing::pointer_type> target_pointer_type = std::dynamic_pointer_cast<typing::pointer_type>(node.get_type().type());
        if (operand_array_type->element_type().type()->is_equivalent_to(*target_pointer_type->pointee_type().type(), m_lowerer.get_platform_info())) {
            return operand;
        }
        throw std::runtime_error(std::format("Currently unsupported type cast: {} to {}", node.get_type().to_string(), node.operand()->get_type().to_string()));
    }

    throw std::runtime_error(std::format("Currently unsupported type cast: {} to {}", node.get_type().to_string(), node.operand()->get_type().to_string()));
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::address_of& node) {
    return std::visit(overloaded{
        [this](const std::unique_ptr<logic::array_index>& operand) -> linear::virtual_register {
            return m_lowerer.compute_lvalue_address(*operand);
        },
        [this](const std::unique_ptr<logic::member_access>& operand) -> linear::virtual_register {
            return m_lowerer.compute_lvalue_address(*operand);
        },
        [this](const std::shared_ptr<logic::variable>& operand) -> linear::virtual_register {
            type_layout_calculator calculator(m_lowerer.get_platform_info());
            if (!operand->must_alloca() && !calculator.must_alloca(operand->get_type())) {
                throw std::runtime_error("Address of variable must be alloca'd on the stack.");
            }
            return m_lowerer.get_var_reg(operand);
        }
    }, node.operand());
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::dereference& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    if (calculator.must_alloca(node.operand()->get_type())) { //we only pass around pointers to alloca'd types
        return m_lowerer.lower_expression(*node.operand());
    }

    auto pointer_type = std::static_pointer_cast<typing::pointer_type>(node.operand()->get_type().type());
    auto layout = calculator(*pointer_type->pointee_type().type());
    auto source_address_reg = m_lowerer.lower_expression(*node.operand());
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
        type_layout_info::get_register_size(layout.size),
        m_lowerer.get_register_class(pointer_type->pointee_type())
    );
    m_lowerer.emit(std::make_unique<linear::load_memory>(
        dest_reg, 
        source_address_reg, 
        0
    ));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::member_access& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto layout = calculator(*node.member().member_type.type());
    
    if (node.base()->get_type().is_same_type<typing::struct_type>() || 
        (node.base()->get_type().is_pointer_of<typing::struct_type>() && node.is_dereference())) {
        // we can safely ignore the dereference case because structs are always passed by their address
        // despite being a pass-by-value type. We always copy them via emit_memcpy when we need to
        // pass them by value, like in a set variable

        auto base_addr = m_lowerer.lower_expression(*node.base());

        if (calculator.must_alloca(node.member().member_type)) { //return a pointer to start of the struct
            auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
                m_lowerer.get_platform_info().pointer_size,
                linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
            );
            m_lowerer.emit(std::make_unique<linear::a2_instruction>(
                linear::MICHAELCC_LINEAR_A_ADD, 
                dest_reg, 
                base_addr,
                node.member().offset
            ));
            return dest_reg;
        }

        auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
            type_layout_info::get_register_size(layout.size),
            m_lowerer.get_register_class(node.member().member_type)
        );
        m_lowerer.emit(std::make_unique<linear::load_memory>(
            dest_reg, 
            base_addr, 
            node.member().offset
        ));
        return dest_reg;
    }
    else if (node.base()->get_type().is_same_type<typing::union_type>()
        || (node.base()->get_type().is_pointer_of<typing::union_type>() && node.is_dereference())) {
        assert(node.member().offset == 0);

        //first if path taken if member type is must alloca
        if ((node.base()->get_type().is_same_type<typing::union_type>() && calculator.must_alloca(node.base()->get_type())) 
            || (node.base()->get_type().is_pointer_of<typing::union_type>() && calculator.must_alloca(node.member().member_type))) {
            auto base_addr = m_lowerer.lower_expression(*node.base());
            if (calculator.must_alloca(node.member().member_type)) {
                return base_addr;
            }
            
            auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
                type_layout_info::get_register_size(layout.size),
                m_lowerer.get_register_class(node.member().member_type)
            );
            m_lowerer.emit(std::make_unique<linear::load_memory>(
                dest_reg, 
                base_addr, 
                node.member().offset
            ));
            return dest_reg;
        }
        else if (node.base()->get_type().is_same_type<typing::union_type>()) {
            return m_lowerer.lower_expression(*node.base());
        }
        else if (node.base()->get_type().is_pointer_of<typing::union_type>()) {
            assert(!calculator.must_alloca(node.member().member_type));
            
            auto base_addr = m_lowerer.lower_expression(*node.base());
            auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
                type_layout_info::get_register_size(layout.size),
                m_lowerer.get_register_class(node.member().member_type)
            );
            m_lowerer.emit(std::make_unique<linear::load_memory>(
                dest_reg, 
                base_addr, 
                node.member().offset
            ));
            return dest_reg;
        }
    }
    
    throw std::runtime_error("Member access on non-struct or union.");
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::array_index& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
 
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(node.base()->get_type().type());

    auto layout = calculator(*array_type->element_type().type());
    auto offset_reg = m_lowerer.m_translation_unit.new_vreg(
        m_lowerer.get_platform_info().pointer_size,
        linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
    );
    m_lowerer.emit(std::make_unique<linear::a2_instruction>(
        linear::MICHAELCC_LINEAR_A_MULTIPLY, 
        offset_reg, 
        m_lowerer.lower_expression(*node.index()), 
        layout.size
    ));

    auto base_addr_reg = m_lowerer.lower_expression(*node.base());
    if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.index().get())) {
        if (calculator.must_alloca(array_type->element_type())) {
            auto address_reg = m_lowerer.m_translation_unit.new_vreg(
                m_lowerer.get_platform_info().pointer_size,
                linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
            );
            m_lowerer.emit(std::make_unique<linear::a2_instruction>(
                linear::MICHAELCC_LINEAR_A_ADD, 
                address_reg, 
                base_addr_reg, 
                integer_constant->value() * layout.size
            ));
            return address_reg;
        }
        
        auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
            type_layout_info::get_register_size(layout.size),
            m_lowerer.get_register_class(array_type->element_type())
        );
        m_lowerer.emit(std::make_unique<linear::load_memory>(
            dest_reg, 
            base_addr_reg, 
            integer_constant->value() * layout.size
        ));
        return dest_reg;
    } else {
        auto address_reg = m_lowerer.m_translation_unit.new_vreg(
            m_lowerer.get_platform_info().pointer_size,
            linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
        );
        m_lowerer.emit(std::make_unique<linear::a_instruction>(
            linear::MICHAELCC_LINEAR_A_ADD, 
            address_reg, 
            base_addr_reg, 
            offset_reg
        ));
        
        if (calculator.must_alloca(array_type->element_type())) {
            return address_reg;
        }

        auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
            type_layout_info::get_register_size(layout.size),
            m_lowerer.get_register_class(array_type->element_type())
        );
        m_lowerer.emit(std::make_unique<linear::load_memory>(
            dest_reg, 
            address_reg, 
            0
        ));
        return dest_reg;
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::array_initializer& node) {
    auto address_reg = m_lowerer.m_translation_unit.new_vreg(
        m_lowerer.get_platform_info().pointer_size,
        linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
    );
    
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(node.element_type().type());
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto elem_layout = calculator(*array_type->element_type().type());
    
    m_lowerer.emit(std::make_unique<linear::alloca_instruction>(
        address_reg, 
        node.initializers().size() * elem_layout.size,
        elem_layout.alignment
    ));

    for (size_t i = 0; i < node.initializers().size(); i++) {
        m_lowerer.lower_at_address(address_reg, node.initializers()[i], i * elem_layout.size);
    }
    return address_reg;
}

void logic_lowerer::lower_allocate_array(linear::virtual_register dest_reg, const logic::allocate_array& node, size_t current_dimension) {
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(node.get_type().type());
    type_layout_calculator calculator(get_platform_info());
    auto layout = calculator(*array_type->element_type().type());

    if (current_dimension == node.dimensions().size() - 1) { // base case for last dimension
        if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.dimensions().at(current_dimension).get())) {
            emit(std::make_unique<linear::alloca_instruction>(
                dest_reg, 
                integer_constant->value() * layout.size, 
                layout.alignment
            ));

            auto fill_value_reg = lower_expression(*node.fill_value());
            assert(static_cast<size_t>(fill_value_reg.reg_size) / 8 == layout.size);
            for (size_t i = 0; i < integer_constant->value(); i++) {
                emit(std::make_unique<linear::store_memory>(
                    dest_reg, 
                    fill_value_reg, 
                    i * layout.size
                ));
            }
        }
        else {
            emit(std::make_unique<linear::valloca_instruction>(
                dest_reg, 
                lower_expression(*node.dimensions()[current_dimension]), 
                layout.alignment
            ));

            // not necessarily correct for types that need to be alloca'd
            emit_memset(
                dest_reg, 
                lower_expression(*node.fill_value()), 
                lower_expression(*node.dimensions()[current_dimension])
            );
        }
    }
    else if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.dimensions().at(current_dimension).get())) {
        auto array_addr_reg = m_translation_unit.new_vreg(
            get_platform_info().pointer_size,
            linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
        );
        emit(std::make_unique<linear::alloca_instruction>(
            array_addr_reg, 
            static_cast<size_t>(get_platform_info().pointer_size) / 8 * integer_constant->value(), 
            std::min<size_t>(
                static_cast<size_t>(get_platform_info().pointer_size) / 8, 
                get_platform_info().max_alignment
            )
        ));

        for (size_t i = 0; i < integer_constant->value(); i++) {
            auto elem_reg = m_translation_unit.new_vreg(
                get_platform_info().pointer_size,
                linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
            );
            lower_allocate_array(elem_reg, node, current_dimension + 1);
            emit(std::make_unique<linear::store_memory>(
                array_addr_reg, 
                elem_reg, 
                i * static_cast<size_t>(get_platform_info().pointer_size) / 8
            ));
        }
    }
    else {
        throw std::runtime_error("Invalid dimension type for allocate_array (first dimension must be an integer constant).");
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::allocate_array& node) {
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
        m_lowerer.get_platform_info().pointer_size,
        linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
    );
    m_lowerer.lower_allocate_array(dest_reg, node, 0);
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::struct_initializer& node) {
    auto& struct_type = node.struct_type();
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto struct_layout = calculator(*struct_type);

    auto dest_address = m_lowerer.m_translation_unit.new_vreg(
        m_lowerer.get_platform_info().pointer_size,
        linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
    );
    m_lowerer.emit(std::make_unique<linear::alloca_instruction>(
        dest_address, 
        struct_layout.size, 
        struct_layout.alignment
    ));

    m_lowerer.lower_struct_initializer(node, dest_address, 0);
    return dest_address;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::union_initializer& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    if (calculator.must_alloca(node.union_type())) {
        auto dest_address = m_lowerer.m_translation_unit.new_vreg(
            m_lowerer.get_platform_info().pointer_size,
            linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER
        );
        auto layout = calculator(*node.union_type());
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(
            dest_address, 
            layout.size, 
            layout.alignment
        ));

        m_lowerer.lower_union_initializer(node, dest_address, 0);
        return dest_address;
    }
    return m_lowerer.lower_expression(*node.initializer());
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::function_call& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());

    linear::function_call::callable callee = std::visit(overloaded{
        [this](const std::shared_ptr<logic::function_definition>& function_definition) -> linear::function_call::callable {
            return function_definition->name();
        },
        [this](const std::unique_ptr<logic::expression>& expression) -> linear::function_call::callable {
            return m_lowerer.lower_expression(*expression);
        }
    }, node.callee());
    
    if (calculator.must_alloca(node.get_type())) {
        throw std::runtime_error("Function return value cannot be alloca'd on the stack.");
    }

    size_t index = 0;
    for (const auto& argument : node.arguments()) {
        auto argument_reg = m_lowerer.lower_expression(*argument);
        m_lowerer.emit(std::make_unique<linear::push_function_argument>(
            linear::function_argument{
                .layout = calculator(*argument->get_type().type()),
                .index = index,
                .register_class = m_lowerer.get_register_class(argument->get_type()),
                .pass_via_stack = calculator.must_alloca(argument->get_type())
            },
            argument_reg
        ));
        index++;
    }

    auto result_layout = calculator(*node.get_type().type());
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(
        type_layout_info::get_register_size(result_layout.size),
        m_lowerer.get_register_class(node.get_type())
    );

    m_lowerer.emit(std::make_unique<linear::function_call>(
        dest_reg, 
        std::move(callee), 
        index
    ));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::conditional_expression& node) {
    auto condition_reg = m_lowerer.lower_expression(*node.condition());
    
    auto true_block_id = m_lowerer.allocate_block_id();
    auto false_block_id = m_lowerer.allocate_block_id();
    auto final_block_id = m_lowerer.allocate_block_id();

    m_lowerer.emit(std::make_unique<linear::branch_condition>(
        condition_reg, true_block_id, false_block_id, false
    ));
    auto current_block_id = m_lowerer.seal_block();

    m_lowerer.begin_block(true_block_id, { current_block_id });
    auto true_result = m_lowerer.lower_expression(*node.then_expression());
    m_lowerer.emit(std::make_unique<linear::branch>(final_block_id));
    true_block_id = m_lowerer.seal_block();
    
    m_lowerer.begin_block(false_block_id, { current_block_id });
    auto false_result = m_lowerer.lower_expression(*node.else_expression());
    m_lowerer.emit(std::make_unique<linear::branch>(final_block_id));
    false_block_id = m_lowerer.seal_block();
    
    m_lowerer.begin_block(final_block_id, { true_block_id, false_block_id });

    assert (true_result.reg_size == false_result.reg_size);
    assert (true_result.reg_class == false_result.reg_class);
    auto final_result = m_lowerer.m_translation_unit.new_vreg(true_result.reg_size, true_result.reg_class);
    m_lowerer.emit(std::make_unique<linear::phi_instruction>(
        final_result, std::vector<linear::var_info>{ 
            linear::var_info{ .vreg = true_result, .block_id = true_block_id },
            linear::var_info{ .vreg = false_result, .block_id = false_block_id }
        }
    ));

    return final_result;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::compound_expression& node) {
    assert(!node.get_type().is_same_type<typing::void_type>());
    
    size_t return_block_id = m_lowerer.allocate_block_id();
    m_lowerer.m_compound_expression_stack.push_back(compound_expression_info{ 
        return_block_id 
    });

    // make sure all code path's compound_return/branch to return block id
    auto final_block_id = m_lowerer.lower_statements(node.control_block()->statements());
    assert(!final_block_id.has_value());
    assert(!m_lowerer.m_current_block.has_value());

    // emit return block id
    auto& compound_info = m_lowerer.m_compound_expression_stack.back();
    assert(compound_info.return_var_info.size() == compound_info.incoming_block_ids.size());
    m_lowerer.begin_block(return_block_id, compound_info.incoming_block_ids);

    type_layout_calculator calculator(m_lowerer.get_platform_info());
    auto return_result_layout = calculator(*node.get_type().type());
    linear::word_size dest_reg_size = calculator.must_alloca(node.get_type()) ? 
        m_lowerer.get_platform_info().pointer_size 
        : type_layout_info::get_register_size(return_result_layout.size);
    linear::register_class dest_reg_class = calculator.must_alloca(node.get_type()) 
        ? linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER 
        : m_lowerer.get_register_class(node.get_type().type());
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(dest_reg_size, dest_reg_class);
    m_lowerer.emit(std::make_unique<linear::phi_instruction>(
        dest_reg, std::move(compound_info.return_var_info)
    ));
    m_lowerer.m_compound_expression_stack.pop_back();
    return dest_reg;
}

linear::virtual_register logic_lowerer::lvalue_lowerer::dispatch(const logic::variable_reference& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    if (!node.get_variable()->must_alloca() && !calculator.must_alloca(node.get_variable()->get_type())) {
        throw std::runtime_error("Address of variable must be alloca'd on the stack.");
    }
    return m_lowerer.get_var_reg(node.get_variable());
}

linear::virtual_register logic_lowerer::lvalue_lowerer::dispatch(const logic::array_index& node) {
    type_layout_calculator calculator(m_lowerer.get_platform_info());
    size_t elem_size = calculator(*node.base()->get_type().type()).size;
    
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(m_lowerer.get_platform_info().pointer_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
    auto base_addr_reg = m_lowerer.lower_expression(*node.base());
    if (auto integer_literal = dynamic_cast<logic::integer_constant*>(node.index().get())) {
        size_t offset = integer_literal->value() * elem_size;
        
        m_lowerer.emit(std::make_unique<linear::a2_instruction>(
            linear::MICHAELCC_LINEAR_A_ADD, 
            dest_reg, 
            base_addr_reg, 
            offset
        ));
        return dest_reg;
    }

    auto index_reg = m_lowerer.lower_expression(*node.index());
    auto offset_reg = m_lowerer.m_translation_unit.new_vreg(m_lowerer.get_platform_info().pointer_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);
    m_lowerer.emit(std::make_unique<linear::a2_instruction>(
        linear::MICHAELCC_LINEAR_A_MULTIPLY, 
        offset_reg, 
        index_reg, 
        elem_size
    ));
    m_lowerer.emit(std::make_unique<linear::a_instruction>(
        linear::MICHAELCC_LINEAR_A_ADD, 
        dest_reg, 
        base_addr_reg, 
        offset_reg
    ));
    return dest_reg;
}

linear::virtual_register logic_lowerer::lvalue_lowerer::dispatch(const logic::member_access& node) {
    auto base_addr_reg = m_lowerer.lower_expression(*node.base());
    auto dest_reg = m_lowerer.m_translation_unit.new_vreg(m_lowerer.get_platform_info().pointer_size, linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER);

    m_lowerer.emit(std::make_unique<linear::a2_instruction>(
        linear::MICHAELCC_LINEAR_A_ADD, 
        dest_reg, 
        base_addr_reg, 
        node.member().offset
    ));
    return dest_reg;
}

void logic_lowerer::statement_lowerer::dispatch(const logic::expression_statement& node) {
    if (auto compound_expression = dynamic_cast<logic::compound_expression*>(node.expression().get())) {
        if (compound_expression->get_type().is_same_type<typing::void_type>()) {
            size_t return_block_id = m_lowerer.allocate_block_id();
            m_lowerer.m_compound_expression_stack.push_back(compound_expression_info{ 
                return_block_id 
            });
        
            // make sure all code path's compound_return/branch to return block id
            auto final_block_id = m_lowerer.lower_statements(compound_expression->control_block()->statements());
            assert(!final_block_id.has_value());
            assert(!m_lowerer.m_current_block.has_value());
        
            // emit return block id
            auto& compound_info = m_lowerer.m_compound_expression_stack.back();
            assert(compound_info.return_var_info.empty());
            m_lowerer.begin_block(return_block_id, compound_info.incoming_block_ids);
            m_lowerer.m_compound_expression_stack.pop_back();
            return;
        }
    }
    m_lowerer.lower_expression(*node.expression());
}

void logic_lowerer::statement_lowerer::dispatch(const logic::return_statement& node) {
    if (node.is_compound_return()) {
        if (node.value()) {
            auto return_reg = m_lowerer.lower_expression(*node.value());
            auto& compound_info = m_lowerer.m_compound_expression_stack.back();
            compound_info.return_var_info.push_back(linear::var_info{
                .vreg = return_reg,
                .block_id = m_lowerer.current_block_id()
            });
            compound_info.incoming_block_ids.push_back(m_lowerer.current_block_id());
            m_lowerer.emit(std::make_unique<linear::branch>(compound_info.return_block_id));
        } 
        else {
            auto& compound_info = m_lowerer.m_compound_expression_stack.back();
            compound_info.incoming_block_ids.push_back(m_lowerer.current_block_id());
            m_lowerer.emit(std::make_unique<linear::branch>(compound_info.return_block_id));
        }
    }
    else {
        if (node.value()) {
            auto virtual_reg = m_lowerer.lower_expression(*node.value());

            linear::register_t return_register_id = m_lowerer.get_platform_info().get_return_register_id(virtual_reg.reg_class, virtual_reg.reg_size);
            auto return_physical_reg = m_lowerer.get_platform_info().get_register_info(return_register_id);
            auto return_vreg = m_lowerer.m_translation_unit.new_vreg(return_physical_reg.size, virtual_reg.reg_class);
            m_lowerer.m_translation_unit.vreg_colors[return_vreg] = return_register_id;

            linear::c_instruction_type copy_type = linear::MICHAELCC_LINEAR_C_COPY_INIT;
            if (virtual_reg.reg_size != return_vreg.reg_size) {
                if (virtual_reg.reg_class == linear::MICHAELCC_REGISTER_CLASS_INTEGER) {
                    auto int_type = std::dynamic_pointer_cast<typing::int_type>(node.value()->get_type().type());
                    copy_type = (int_type && int_type->is_signed())
                        ? linear::MICHAELCC_LINEAR_C_SEXT_OR_TRUNC
                        : linear::MICHAELCC_LINEAR_C_ZEXT_OR_TRUNC;
                } else {
                    copy_type = (virtual_reg.reg_size == linear::MICHAELCC_WORD_SIZE_UINT32)
                        ? linear::MICHAELCC_LINEAR_C_FLOAT32_TO_FLOAT64
                        : linear::MICHAELCC_LINEAR_C_FLOAT64_TO_FLOAT32;
                }
            }
            m_lowerer.emit(std::make_unique<linear::c_instruction>(copy_type, return_vreg, virtual_reg));
        }

        m_lowerer.emit(std::make_unique<linear::function_return>());
    }
    m_lowerer.seal_block();
}

void logic_lowerer::statement_lowerer::dispatch(const logic::if_statement& node) {
    auto condition_reg = m_lowerer.lower_expression(*node.condition());
    
    auto finish_block_id = m_lowerer.allocate_block_id();
    auto current_block_id = m_lowerer.current_block_id();
    auto true_block_id = m_lowerer.allocate_block_id();

    if (node.else_body()) {
        auto false_block_id = m_lowerer.allocate_block_id();
        m_lowerer.emit(std::make_unique<linear::branch_condition>(
            condition_reg, true_block_id, false_block_id, false
        ));
        current_block_id = m_lowerer.seal_block();

        // compile true block body
        m_lowerer.begin_block(true_block_id, { current_block_id });
        auto final_true_block_id = m_lowerer.lower_statements(node.then_body()->statements());

        if (final_true_block_id.has_value()) {
            m_lowerer.emit(std::make_unique<linear::branch>(finish_block_id));
            m_lowerer.seal_block();
        }

        // compile false block body
        m_lowerer.begin_block(false_block_id, { current_block_id });
        auto final_false_block_id = m_lowerer.lower_statements(node.else_body()->statements());

        if (final_false_block_id.has_value()) {
            m_lowerer.emit(std::make_unique<linear::branch>(finish_block_id));
            m_lowerer.seal_block();
        }

        std::vector<size_t> incoming_block_ids;
        incoming_block_ids.reserve(2);
        if (final_true_block_id.has_value()) { incoming_block_ids.push_back(*final_true_block_id); }
        if (final_false_block_id.has_value()) { incoming_block_ids.push_back(*final_false_block_id); }

        if (!incoming_block_ids.empty()) {
            m_lowerer.begin_block(finish_block_id, incoming_block_ids);
        }
    }
    else {
        m_lowerer.emit(std::make_unique<linear::branch_condition>(
            condition_reg, true_block_id, finish_block_id, false
        ));
        current_block_id = m_lowerer.seal_block();

        // compile true block body
        m_lowerer.begin_block(true_block_id, { current_block_id });
        auto final_true_block_id = m_lowerer.lower_statements(node.then_body()->statements());
        
        if (final_true_block_id.has_value()) {
            m_lowerer.emit(std::make_unique<linear::branch>(finish_block_id));
            m_lowerer.seal_block();
        }
        std::vector<size_t> incoming_block_ids;
        incoming_block_ids.reserve(2);
        if (final_true_block_id.has_value()) { incoming_block_ids.push_back(*final_true_block_id); }
        incoming_block_ids.push_back(current_block_id);
        m_lowerer.begin_block(finish_block_id, incoming_block_ids);
    }
}

void logic_lowerer::statement_lowerer::dispatch(const logic::loop_statement& node) {
    if (node.check_condition_first()) {
        auto loop_condition_begin_block_id = m_lowerer.allocate_block_id();
        auto loop_body_begin_block_id = m_lowerer.allocate_block_id();

        m_lowerer.emit(std::make_unique<linear::branch>(loop_condition_begin_block_id));
        auto current_block_id = m_lowerer.seal_block();

        // compile condition block
        m_lowerer.begin_block(loop_condition_begin_block_id, { current_block_id }, true);
        auto loop_finish_block_id = m_lowerer.m_loop_infos[loop_condition_begin_block_id].finish_block_id;
        
        auto loop_condition_reg = m_lowerer.lower_expression(*node.condition());
        m_lowerer.emit(std::make_unique<linear::branch_condition>(
            loop_condition_reg, loop_body_begin_block_id, loop_finish_block_id, true
        ));
        auto loop_condition_end_block_id = m_lowerer.seal_block();

        // compile body block
        m_lowerer.begin_block(loop_body_begin_block_id, { loop_condition_end_block_id });
        auto loop_body_end_block_id = m_lowerer.lower_statements(node.body()->statements());

        if (loop_body_end_block_id.has_value()) {
            m_lowerer.emit(std::make_unique<linear::branch>(loop_condition_begin_block_id));
            m_lowerer.seal_block();
    
            m_lowerer.recurse_block(loop_body_end_block_id.value(), loop_condition_begin_block_id);
        }
        m_lowerer.pop_loop_stack();

        //compile finish block
        std::vector<size_t> incoming_block_ids;
        incoming_block_ids.reserve(1 + m_lowerer.m_loop_infos[loop_condition_begin_block_id].incoming_break_block_ids.size());
        incoming_block_ids.push_back(loop_condition_end_block_id);
        incoming_block_ids.insert(
            incoming_block_ids.end(), 
            m_lowerer.m_loop_infos[loop_condition_begin_block_id].incoming_break_block_ids.begin(), 
            m_lowerer.m_loop_infos[loop_condition_begin_block_id].incoming_break_block_ids.end()
        );
        m_lowerer.begin_block(loop_finish_block_id, incoming_block_ids);
    }
    else {
        size_t loop_block_begin_id = m_lowerer.allocate_block_id();
        size_t loop_condition_begin_id = m_lowerer.allocate_block_id();

        m_lowerer.emit(std::make_unique<linear::branch>(loop_block_begin_id));
        size_t current_block_id = m_lowerer.seal_block();

        // compile loop block
        m_lowerer.begin_block(loop_block_begin_id, { current_block_id }, true);
        auto loop_block_finish_id = m_lowerer.m_loop_infos[loop_block_begin_id].finish_block_id;
        m_lowerer.m_loop_infos[loop_block_begin_id].alternate_continue_target_block_id = loop_condition_begin_id;
        auto loop_block_end_block_id = m_lowerer.lower_statements(node.body()->statements());

        std::vector<size_t> condition_incoming_block_ids;
        condition_incoming_block_ids.reserve(1 + m_lowerer.m_loop_infos[loop_block_begin_id].incoming_continue_block_ids.size());
        std::vector<size_t> finish_incoming_block_ids;
        finish_incoming_block_ids.reserve(1 + m_lowerer.m_loop_infos[loop_block_begin_id].incoming_break_block_ids.size());

        if (loop_block_end_block_id.has_value()) {
            m_lowerer.emit(std::make_unique<linear::branch>(loop_condition_begin_id));
            m_lowerer.seal_block();
            condition_incoming_block_ids.push_back(loop_block_end_block_id.value());
        }
        m_lowerer.pop_loop_stack();

        // compile condition block
        condition_incoming_block_ids.insert(
            condition_incoming_block_ids.end(), 
            m_lowerer.m_loop_infos[loop_block_begin_id].incoming_continue_block_ids.begin(), 
            m_lowerer.m_loop_infos[loop_block_begin_id].incoming_continue_block_ids.end()
        );
        if (!condition_incoming_block_ids.empty()) {
            m_lowerer.begin_block(loop_condition_begin_id, condition_incoming_block_ids);
            auto loop_condition_reg = m_lowerer.lower_expression(*node.condition());
            m_lowerer.emit(std::make_unique<linear::branch_condition>(
                loop_condition_reg, loop_block_begin_id, loop_block_finish_id, true
            ));
            auto loop_condition_end_block_id = m_lowerer.seal_block();
            m_lowerer.recurse_block(loop_condition_end_block_id, loop_block_begin_id);
            finish_incoming_block_ids.push_back(loop_condition_end_block_id);
        }

        //compile finish block
        finish_incoming_block_ids.insert(
            finish_incoming_block_ids.end(), 
            m_lowerer.m_loop_infos[loop_block_begin_id].incoming_break_block_ids.begin(), 
            m_lowerer.m_loop_infos[loop_block_begin_id].incoming_break_block_ids.end()
        );
        if (!finish_incoming_block_ids.empty()) {
            m_lowerer.begin_block(loop_block_finish_id, finish_incoming_block_ids);
        }
    }
}

void logic_lowerer::statement_lowerer::dispatch(const logic::break_statement& node) {
    if (node.loop_depth() > m_lowerer.current_loop_depth()) {
        throw std::runtime_error("Cannot break out of the outer loop");
    }

    auto& loop_info = m_lowerer.m_loop_infos.at(m_lowerer.m_loop_stack[m_lowerer.current_loop_depth() - node.loop_depth()]);
    m_lowerer.emit(std::make_unique<linear::branch>(loop_info.finish_block_id));
    loop_info.incoming_break_block_ids.push_back(m_lowerer.current_block_id());
    m_lowerer.seal_block();
}

void logic_lowerer::statement_lowerer::dispatch(const logic::continue_statement& node) {
    if (node.loop_depth() > m_lowerer.current_loop_depth()) {
        throw std::runtime_error("Cannot break out of the outer loop");
    }

    auto& loop_info = m_lowerer.m_loop_infos.at(m_lowerer.m_loop_stack[m_lowerer.current_loop_depth() - node.loop_depth()]);
    if (loop_info.alternate_continue_target_block_id.has_value()) { //do-while loop continue
        auto target_block_id = loop_info.alternate_continue_target_block_id.value();
        m_lowerer.emit(std::make_unique<linear::branch>(target_block_id));
        loop_info.incoming_continue_block_ids.push_back(m_lowerer.current_block_id());
        m_lowerer.seal_block();
    }
    else { //regular while loop continue
        auto target_block_id = loop_info.block_id; //condition at start of loop
        m_lowerer.emit(std::make_unique<linear::branch>(target_block_id));
        auto current_block_id = m_lowerer.seal_block();

        m_lowerer.recurse_block(current_block_id, target_block_id);
    }
}

void logic_lowerer::statement_lowerer::dispatch(const logic::statement_block& node) {
    m_lowerer.lower_statements(node.control_block()->statements());
}

void logic_lowerer::lower_function(const logic::function_definition& node) {
    std::unordered_map<std::string, linear::function_parameter> parameter_map;
    parameter_map.reserve(node.parameters().size());
    type_layout_calculator calculator(get_platform_info());
    size_t index = 0;
    for (const auto& parameter : node.parameters()) {
        parameter_map.emplace(parameter->name(), linear::function_parameter{
            .name = parameter->name(),
            .layout = calculator(*parameter->get_type().type()),
            .index = index,
            .register_class = get_register_class(parameter->get_type()),
            .pass_via_stack = calculator.must_alloca(parameter->get_type())
        });
        index++;
    }

    m_current_function = function_builder{
        .name = node.name(),
        .parameters = std::move(parameter_map)
    };

    auto entry_block_id = allocate_block_id();
    begin_block(entry_block_id, {});

    // declare registers for params that need to be passed by value, and loaded directly in registers
    for (const auto& [name, parameter] : m_current_function->parameters) {
        if (!parameter.pass_via_stack) {
            auto var_reg = m_translation_unit.new_vreg(type_layout_info::get_register_size(parameter.layout.size), parameter.register_class);
            emit(std::make_unique<linear::load_parameter>(var_reg, parameter));

            auto var = std::find_if(node.parameters().begin(), node.parameters().end(), [&](const auto& p) { return p->name() == name; });
            m_current_block->var_info.m_variable_to_vreg[*var] = linear::var_info{ .vreg = var_reg, .block_id = current_block_id() };
        }
    }

    auto final_block_id = lower_statements(node.statements());
    assert(!final_block_id.has_value()); //all code paths must return and hence seal off the block
    assert(!m_current_block.has_value());

    std::vector<linear::function_parameter> parameters;
    parameters.reserve(m_current_function->parameters.size());
    for (const auto& [name, parameter] : m_current_function->parameters) {
        parameters.emplace_back(std::move(parameter));
    }

    m_translation_unit.function_definitions.emplace_back(std::make_unique<linear::function_definition>(
        node.name(), 
        entry_block_id, 
        std::move(parameters)
    ));
    m_current_function = std::nullopt;
}

void logic_lowerer::lower_static_variable_declaration(const logic::variable_declaration& declaration) {
    linear::static_storage::is_default_initialized is_default_initialized(get_platform_info());
    auto default_layout = is_default_initialized(*declaration.initializer());
    if (default_layout.has_value()) {
        m_translation_unit.static_sections.bss_allocations.emplace_back(linear::static_storage::bss_allocation{
            .label = declaration.variable()->name(),
            .layout = *default_layout
        });
    }
    else { //allocate data section
        linear::static_storage::data_section_builder builder(get_platform_info(), declaration.variable()->name());
        auto data_allocations = builder.build(*declaration.initializer());
        for (auto& data_allocation : data_allocations) {
            m_translation_unit.static_sections.data_allocations.emplace_back(std::move(data_allocation));
        }
    }
}

size_t logic_lowerer::seal_block() {
    if (!m_current_block) throw std::runtime_error("Cannot seal a block that does not exist");
    auto& cb = *m_current_block;
    
    std::vector<size_t> successor_block_ids;
    std::vector<size_t> predecessor_block_ids;

    if (auto branch = dynamic_cast<const linear::branch*>(cb.instructions.back().get())) {
        successor_block_ids.push_back(branch->next_block_id());
    }
    else if (auto branch_condition = dynamic_cast<const linear::branch_condition*>(cb.instructions.back().get())) {
        successor_block_ids.push_back(branch_condition->if_true_block_id());
        successor_block_ids.push_back(branch_condition->if_false_block_id());
    }
    
    m_translation_unit.blocks.emplace(cb.id, linear::basic_block(
        cb.id, 
        std::move(cb.instructions),
        std::move(successor_block_ids)
    ));
    m_finished_block_var_ctx.emplace(cb.id, std::move(cb.var_info));
    size_t return_id = cb.id;
    m_current_block.reset();
    return return_id;
}

void logic_lowerer::lower(const logic::translation_unit& translation_unit) {
    for (const auto& declaration : translation_unit.static_variable_declarations()) {
        lower_static_variable_declaration(declaration);
    }
    for (const auto& sym : translation_unit.global_context()->symbols()) {
        auto* func = dynamic_cast<logic::function_definition*>(sym.get());
        if (func) {
            lower_function(*func);
        }
    }

    // compute predecessor based on all basic block predecessors
    for (const auto& [block_id, block] : m_translation_unit.blocks) {
        for (const auto& successor_block_id : block.successor_block_ids()) {
            m_translation_unit.blocks.at(successor_block_id).add_predecessor_block_id(block_id);
        }
    }

    linear::compute_dominators(m_translation_unit);
}