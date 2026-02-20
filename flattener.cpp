#include "linear/flatten.hpp"
#include "linear/ir.hpp"
#include "logic/ir.hpp"
#include "logic/type_info.hpp"
#include "logic/typing.hpp"
#include <bit>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <variant>
#include <stdexcept>
#include <cassert>

using namespace michaelcc;

logic_lowerer::block_var_ctx logic_lowerer::reconcile_var_regs(const std::vector<size_t>& incoming_block_ids, size_t incoming_block_id) {
    block_var_ctx result;

    std::unordered_set<std::shared_ptr<logic::variable>> seen_variables;
    for (size_t block_id : incoming_block_ids) {
        if (incoming_block_id == block_id) {
            continue;
        }
        
        const auto& block_var_ctx = m_finished_block_var_ctx.at(block_id);
        for (const auto& [variable, vregs] : block_var_ctx.m_variable_to_vreg) {
            seen_variables.insert(variable);
        }
    }

    for (const std::shared_ptr<logic::variable>& variable : seen_variables) {
        std::vector<linear::var_info> vregs;
        for (size_t block_id : incoming_block_ids) {
            if (incoming_block_id == block_id) {
                continue;
            }

            const auto& block_var_ctx = m_finished_block_var_ctx.at(block_id);
            auto it = block_var_ctx.m_variable_to_vreg.find(variable);
            if (it != block_var_ctx.m_variable_to_vreg.end()) {
                vregs.insert(vregs.end(), it->second.begin(), it->second.end());
            }
        }
        result.m_variable_to_vreg[variable] = vregs;
    }    
    return result;
}

void logic_lowerer::emit_phi_all() {
    std::unordered_map<std::shared_ptr<logic::variable>, linear::phi_instruction*> init_phi_nodes;
    for (const auto& [variable, vregs] : m_current_block->var_info.m_variable_to_vreg) {
        type_layout_calculator calculator(m_platform_info);
        auto dest_reg = new_vreg(
            calculator(*variable->get_type().type()).size * 8,
            variable->must_use_register(),
            variable->name()
        );
        auto phi_node = std::make_unique<linear::phi_instruction>(dest_reg, std::vector<linear::var_info>(vregs));
        init_phi_nodes[variable] = phi_node.get();
        emit(std::move(phi_node));
        m_current_block->var_info.m_variable_to_vreg[variable] = { linear::var_info{ .vreg = dest_reg, .block_id = current_block_id() } };
    }
    m_loop_infos[current_block_id()] = loop_info{ 
        .id = current_block_id(), 
        .original_var_info = m_current_block->var_info.m_variable_to_vreg, 
        .init_phi_nodes = std::move(init_phi_nodes)
    };
}

void logic_lowerer::recurse_block(size_t head_block_id, size_t tail_block_id) {
    auto& tail_block_var_ctx = m_finished_block_var_ctx.at(tail_block_id);
    auto& head_init_info = m_loop_infos.at(head_block_id);

    for (const auto& [variable, vregs] : tail_block_var_ctx.m_variable_to_vreg) {
        auto it = head_init_info.init_phi_nodes.find(variable);
        if (it != head_init_info.init_phi_nodes.end()) {
            it->second->augment_values(vregs);
        }
    }
}

linear::virtual_register logic_lowerer::get_var_reg(const std::shared_ptr<logic::variable>& variable) {
    std::vector<linear::var_info> vregs = m_current_block->var_info.m_variable_to_vreg.at(variable);
    
    // if only one definition we are gtg
    if (vregs.size() == 1) {
        return vregs.at(0).vreg;
    }

    // if multiple definitions we need to reconcile with phi node
    type_layout_calculator calculator(m_platform_info);
    auto dest_reg = new_vreg(
        calculator(*variable->get_type().type()).size * 8,
        variable->must_use_register(),
        variable->name()
    );
    
    emit(std::make_unique<linear::phi_instruction>(dest_reg, std::move(vregs)));

    // update the current block's var info
    m_current_block->var_info.m_variable_to_vreg[variable] = { linear::var_info{ .vreg = dest_reg, .block_id = current_block_id() } };

    return dest_reg;
}

void logic_lowerer::emit_iloop(linear::virtual_register count, std::function<void(linear::virtual_register)> body) {
    auto initial_state = new_vreg(m_platform_info.int_size * 8);
    emit(std::make_unique<linear::init_register>(initial_state, 0));

    size_t old_block_id = current_block_id();
    size_t loop1_block_id = allocate_block_id();
    size_t loop2_block_id = allocate_block_id();
    size_t finish_block_id = allocate_block_id();

    emit(std::make_unique<linear::branch>(loop1_block_id));
    seal_block(); //end current block

    begin_block(loop1_block_id, { old_block_id, loop2_block_id });
    auto iterator_state = new_vreg(m_platform_info.int_size * 8);
    auto post_increment_state = new_vreg(m_platform_info.int_size * 8);

    emit(std::make_unique<linear::phi_instruction>(
        iterator_state, std::vector<linear::var_info>{ 
            linear::var_info{ .vreg = initial_state, .block_id = old_block_id },
            linear::var_info{ .vreg = post_increment_state, .block_id = loop2_block_id }
        }
    ));

    auto cond_check_status = new_vreg(m_platform_info.int_size * 8);
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
        auto offset_reg = new_vreg(m_platform_info.int_size * 8);
        emit(std::make_unique<linear::a2_instruction>(
            linear::MICHAELCC_LINEAR_A_MULTIPLY, offset_reg, iterator_state, 
            value.size_bits / 8
        ));
        auto address_reg = new_vreg(m_platform_info.pointer_size * 8);
        emit(std::make_unique<linear::a_instruction>(
            linear::MICHAELCC_LINEAR_A_ADD, address_reg, dest, offset_reg
        ));
        emit(std::make_unique<linear::store_memory>(address_reg, value, 0));
    });
}

void logic_lowerer::emit_memcpy(linear::virtual_register dest, linear::virtual_register src, size_t size_bytes, size_t offset) {
    for (size_t i = 0; i < size_bytes; i += m_platform_info.pointer_size) {
        auto elem_reg = new_vreg(m_platform_info.char_size);    
        emit(std::make_unique<linear::load_memory>(elem_reg, src, i + offset));
        emit(std::make_unique<linear::store_memory>(dest, elem_reg, i + offset));
    }
}

void logic_lowerer::lower_struct_initializer(const logic::struct_initializer& node, linear::virtual_register dest_address, size_t offset) {
    auto& struct_type = node.struct_type();
    
    type_layout_calculator calculator(m_platform_info);
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
    type_layout_calculator calculator(m_platform_info);
    assert(node.target_member().offset == 0);
    assert(calculator.must_alloca(node.union_type()));

    lower_at_address(dest_address, node.initializer(), 0);
}

void logic_lowerer::lower_at_address(linear::virtual_register dest_address, const std::unique_ptr<logic::expression>& initializer, size_t offset) {
    type_layout_calculator calculator(m_platform_info);
    if(calculator.must_alloca(initializer->get_type().type())) {
        if (auto struct_initializer = dynamic_cast<logic::struct_initializer*>(initializer.get())) {
            lower_struct_initializer(*struct_initializer, dest_address, offset);
        }
        else if (auto union_initializer = dynamic_cast<logic::union_initializer*>(initializer.get())) {
            lower_union_initializer(*union_initializer, dest_address, offset);
        }
        else {
            auto layout = calculator(*initializer->get_type().type());
            auto src_addr = lower_expression(*initializer);
            emit_memcpy(
                dest_address, 
                src_addr, 
                layout.size, 
                offset
            );
        }
    }
    else {
        auto evaled_src_reg = lower_expression(*initializer);
        emit(std::make_unique<linear::store_memory>(
            dest_address, 
            evaled_src_reg, 
            offset
        ));
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::integer_constant& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    auto dest_reg = make_dest_reg(calculator(*node.get_type().type()).size * 8);
    
    m_lowerer.emit(std::make_unique<linear::init_register>(
        dest_reg, std::bit_cast<uint64_t>(node.value())
    ));

    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::floating_constant& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    auto dest_reg = make_dest_reg(calculator(*node.get_type().type()).size * 8);

    m_lowerer.emit(std::make_unique<linear::init_register>(
        dest_reg, std::bit_cast<uint64_t>(node.value())
    ));

    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::string_constant& node) {
    std::ostringstream ss;
    ss << "@string_" << node.index();
    
    auto dest_reg = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::load_effective_address>(
        dest_reg, ss.str()
    ));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::variable_reference& node) {
    auto variable = node.get_variable();
    auto var_reg = m_lowerer.get_var_reg(variable);
    type_layout_calculator calculator(m_lowerer.m_platform_info);

    if (variable->must_alloca() || calculator.must_alloca(variable->get_type())) {
        type_layout_calculator calculator(m_lowerer.m_platform_info);
        if (calculator(*variable->get_type().type()).size * 8 <= m_lowerer.m_platform_info.register_size) {
            auto dest_reg = make_dest_reg(calculator(*variable->get_type().type()).size * 8);
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
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    auto var_reg = m_lowerer.get_var_reg(node.variable());
    
    if (node.variable()->must_alloca() || calculator.must_alloca(node.variable()->get_type())) {
        if (calculator.must_alloca(node.variable()->get_type())) {
            m_lowerer.lower_at_address(var_reg, node.value(), 0);
            return var_reg;
        }
        auto evaled_src_reg = m_lowerer.lower_expression(
            *node.value(),
            m_dest_reg_use_register,
            m_dest_reg_name
        );
        m_lowerer.emit(std::make_unique<linear::store_memory>(
            var_reg, 
            evaled_src_reg, 
            0
        ));
        return evaled_src_reg;
    }

    auto new_reg = m_lowerer.lower_expression(
        *node.value(),
        node.variable()->must_use_register(),
        node.variable()->name()
    );
    assert(new_reg != var_reg);

    m_lowerer.m_current_block->var_info.m_variable_to_vreg[node.variable()] = { linear::var_info{ 
        .vreg = new_reg, 
        .block_id = m_lowerer.current_block_id() 
    } };

    return new_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::set_address& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);

    if (calculator.must_alloca(node.destination()->get_type())) {
        auto dest_addr = m_lowerer.lower_expression(
            *node.destination(),
            m_dest_reg_use_register,
            m_dest_reg_name
        );
        m_lowerer.lower_at_address(dest_addr, node.value(), 0);
        return dest_addr;
    }
    else {
        auto dest_addr = m_lowerer.lower_expression(*node.destination());
        auto evaled_src_reg = m_lowerer.lower_expression(
            *node.value(),
            m_dest_reg_use_register,
            m_dest_reg_name
        );
        m_lowerer.emit(std::make_unique<linear::store_memory>(
            dest_addr, 
            evaled_src_reg, 
            0
        ));
        return evaled_src_reg;
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::function_reference& node) {
    auto dest_reg = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::load_effective_address>(dest_reg, node.get_function()->name()));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::increment_operator& node) {
    bool use_one = node.get_operator() == MICHAELCC_TOKEN_INCREMENT || node.get_operator() == MICHAELCC_TOKEN_DECREMENT;
    linear::a_instruction_type type;
    switch (node.get_operator()) {
    case MICHAELCC_TOKEN_INCREMENT:
    case MICHAELCC_TOKEN_INCREMENT_BY:
        type = linear::a_instruction_type::MICHAELCC_LINEAR_A_ADD;
        break;
    case MICHAELCC_TOKEN_DECREMENT:
    case MICHAELCC_TOKEN_DECREMENT_BY:
        type = linear::a_instruction_type::MICHAELCC_LINEAR_A_SUBTRACT;
        break;
    default:
        throw std::runtime_error("Invalid increment operator");
    }

    if (std::holds_alternative<std::shared_ptr<logic::variable>>(node.destination())) {
        auto variable = std::get<std::shared_ptr<logic::variable>>(node.destination());
        if (!variable->must_alloca()) {
            auto var_reg = m_lowerer.get_var_reg(variable);
            auto new_var_reg = make_dest_reg(var_reg.size_bits);

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
            
            m_lowerer.m_current_block->var_info.m_variable_to_vreg[variable] = { linear::var_info{ .vreg = new_var_reg, .block_id = m_lowerer.current_block_id() } };
            return var_reg;
        }
    }

    type_layout_calculator calculator(m_lowerer.m_platform_info);
    size_t var_size = calculator(*node.get_type().type()).size * 8;

    auto value_addr = std::visit(overloaded{
        [this](const std::unique_ptr<logic::expression>& expr) -> linear::virtual_register {
            return m_lowerer.compute_lvalue_address(*expr);
        },
        [this](const std::shared_ptr<logic::variable>& variable) -> linear::virtual_register {
            return m_lowerer.get_var_reg(variable);
        }
    }, node.destination());

    auto current_value_reg = make_dest_reg(var_size);
    m_lowerer.emit(std::make_unique<linear::load_memory>(
        current_value_reg, 
        value_addr, 
        0 // offset
    ));
    auto incremented_reg = m_lowerer.new_vreg(var_size);
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
        0 // offset
    ));
    return current_value_reg;
}

static linear::a_instruction_type token_to_a_type(token_type op) {
    switch (op) {
        case MICHAELCC_TOKEN_PLUS:           return linear::MICHAELCC_LINEAR_A_ADD;
        case MICHAELCC_TOKEN_MINUS:          return linear::MICHAELCC_LINEAR_A_SUBTRACT;
        case MICHAELCC_TOKEN_ASTERISK:       return linear::MICHAELCC_LINEAR_A_MULTIPLY;
        case MICHAELCC_TOKEN_SLASH:          return linear::MICHAELCC_LINEAR_A_DIVIDE;
        case MICHAELCC_TOKEN_MODULO:         return linear::MICHAELCC_LINEAR_A_MODULO;
        case MICHAELCC_TOKEN_BITSHIFT_LEFT:  return linear::MICHAELCC_LINEAR_A_SHIFT_LEFT;
        case MICHAELCC_TOKEN_BITSHIFT_RIGHT: return linear::MICHAELCC_LINEAR_A_SHIFT_RIGHT;
        case MICHAELCC_TOKEN_AND:            return linear::MICHAELCC_LINEAR_A_BITWISE_AND;
        case MICHAELCC_TOKEN_OR:             return linear::MICHAELCC_LINEAR_A_BITWISE_OR;
        case MICHAELCC_TOKEN_CARET:          return linear::MICHAELCC_LINEAR_A_BITWISE_XOR;
        case MICHAELCC_TOKEN_DOUBLE_AND:     return linear::MICHAELCC_LINEAR_A_AND;
        case MICHAELCC_TOKEN_DOUBLE_OR:      return linear::MICHAELCC_LINEAR_A_OR;
        case MICHAELCC_TOKEN_EQUALS:         return linear::MICHAELCC_LINEAR_A_COMPARE_EQUAL;
        case MICHAELCC_TOKEN_NOT_EQUALS:     return linear::MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL;
        case MICHAELCC_TOKEN_LESS:           return linear::MICHAELCC_LINEAR_A_COMPARE_LESS_THAN;
        case MICHAELCC_TOKEN_LESS_EQUAL:     return linear::MICHAELCC_LINEAR_A_COMPARE_LESS_THAN_OR_EQUAL;
        case MICHAELCC_TOKEN_MORE:           return linear::MICHAELCC_LINEAR_A_COMPARE_GREATER_THAN;
        case MICHAELCC_TOKEN_MORE_EQUAL:     return linear::MICHAELCC_LINEAR_A_COMPARE_GREATER_THAN_OR_EQUAL;
        default: throw std::runtime_error("Unsupported binary operator for a_instruction");
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::arithmetic_operator& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    size_t result_size = calculator(*node.get_type().type()).size * 8;

    linear::virtual_register left = m_lowerer.lower_expression(*node.left());
    linear::virtual_register right = m_lowerer.lower_expression(*node.right());

    linear::a_instruction_type type = token_to_a_type(node.get_operator());
    auto result_reg = make_dest_reg(result_size);
    m_lowerer.emit(std::make_unique<linear::a_instruction>(type, result_reg, left, right));
    return result_reg;
}

static linear::u_instruction_type token_to_u_type(token_type op) {
    switch (op) {
        case MICHAELCC_TOKEN_MINUS: return linear::MICHAELCC_LINEAR_U_NEGATE;
        case MICHAELCC_TOKEN_NOT: return linear::MICHAELCC_LINEAR_U_NOT;
        case MICHAELCC_TOKEN_TILDE: return linear::MICHAELCC_LINEAR_U_BITWISE_NOT;
        default: throw std::runtime_error("Unsupported unary operator");
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::unary_operation& node) {
    auto operand = m_lowerer.lower_expression(*node.operand());
    type_layout_calculator calculator(m_lowerer.m_platform_info);

    size_t bits = calculator(*node.get_type().type()).size * 8;
    auto dest = make_dest_reg(bits);

    auto u_operator = token_to_u_type(node.get_operator());
    m_lowerer.emit(std::make_unique<linear::u_instruction>(u_operator, dest, operand));
    return dest;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::type_cast& node) {
    if (node.get_type().is_same_type<typing::float_type>() && node.operand()->get_type().is_same_type<typing::int_type>()) {
        throw std::runtime_error("Currently unsupported type cast: float to integer.");
    }
    if (node.get_type().is_same_type<typing::int_type>() && node.operand()->get_type().is_same_type<typing::float_type>()) {
        throw std::runtime_error("Currently unsupported type cast: integer to float.");
    }
    return m_lowerer.lower_expression(
        *node.operand(),
        m_dest_reg_use_register,
        m_dest_reg_name
    );
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
            type_layout_calculator calculator(m_lowerer.m_platform_info);
            if (!operand->must_alloca() || !calculator.must_alloca(operand->get_type())) {
                throw std::runtime_error("Address of variable must be alloca'd on the stack.");
            }
            return m_lowerer.get_var_reg(operand);
        }
    }, node.operand());
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::dereference& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    if (calculator.must_alloca(node.operand()->get_type())) { //we only pass around pointers to alloca'd types
        return m_lowerer.lower_expression(*node.operand());
    }

    size_t object_size = calculator(*node.operand()->get_type().type()).size * 8;

    auto source_address_reg = m_lowerer.lower_expression(*node.operand());
    auto dest_reg = make_dest_reg(object_size);
    m_lowerer.emit(std::make_unique<linear::load_memory>(
        dest_reg, 
        source_address_reg, 
        0 // offset
    ));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::member_access& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    if (node.base()->get_type().is_same_type<typing::struct_type>() || 
        (node.base()->get_type().is_pointer_of<typing::struct_type>() && node.is_dereference())) {
        // we can safely ignore the dereference case because structs are always passed by their address
        // despite being a pass-by-value type. We always copy them via emit_memcpy when we need to
        // pass them by value, like in a set variable

        assert(calculator.must_alloca(node.base()->get_type()));

        auto base_addr = m_lowerer.lower_expression(*node.base());

        if (calculator.must_alloca(node.member().member_type)) { //return a pointer to start of the struct
            auto dest_reg = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
            m_lowerer.emit(std::make_unique<linear::a2_instruction>(
                linear::MICHAELCC_LINEAR_A_ADD, 
                dest_reg, 
                base_addr,
                node.member().offset
            ));
            return dest_reg;
        }

        size_t object_size = calculator(*node.member().member_type.type()).size * 8;

        auto dest_reg = make_dest_reg(object_size);
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
            auto base_addr = m_lowerer.lower_expression(
                *node.base(),
                m_dest_reg_use_register,
                m_dest_reg_name
            );
            if (calculator.must_alloca(node.member().member_type)) {
                return base_addr;
            }
            
            size_t object_size = calculator(*node.member().member_type.type()).size * 8;
            auto dest_reg = make_dest_reg(object_size);
            m_lowerer.emit(std::make_unique<linear::load_memory>(
                dest_reg, 
                base_addr, 
                node.member().offset
            ));
            return dest_reg;
        }
        else if (node.base()->get_type().is_same_type<typing::union_type>()) {
            return m_lowerer.lower_expression(
                *node.base(),
                m_dest_reg_use_register,
                m_dest_reg_name
            );
        }
        else if (node.base()->get_type().is_pointer_of<typing::union_type>()) {
            assert(!calculator.must_alloca(node.member().member_type));
            
            auto base_addr = m_lowerer.lower_expression(*node.base());
            size_t object_size = calculator(*node.member().member_type.type()).size * 8;

            auto dest_reg = make_dest_reg(object_size);
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
    type_layout_calculator calculator(m_lowerer.m_platform_info);
 
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(node.base()->get_type().type());

    size_t elem_size = calculator(*array_type->element_type().type()).size * 8;
    auto offset_reg = m_lowerer.new_vreg(elem_size);
    m_lowerer.emit(std::make_unique<linear::a2_instruction>(
        linear::MICHAELCC_LINEAR_A_MULTIPLY, 
        offset_reg, 
        m_lowerer.lower_expression(*node.index()), 
        elem_size
    ));

    auto base_addr_reg = m_lowerer.lower_expression(*node.base());
    if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.index().get())) {
        if (calculator.must_alloca(array_type->element_type())) {
            auto address_reg = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
            m_lowerer.emit(std::make_unique<linear::a2_instruction>(
                linear::MICHAELCC_LINEAR_A_ADD, 
                address_reg, 
                base_addr_reg, 
                integer_constant->value() * elem_size
            ));
            return address_reg;
        }
        
        auto dest_reg = make_dest_reg(elem_size);
        m_lowerer.emit(std::make_unique<linear::load_memory>(
            dest_reg, 
            base_addr_reg, 
            integer_constant->value() * elem_size
        ));
        return dest_reg;
    } else {
        if (calculator.must_alloca(array_type->element_type())) {
            auto address_reg = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                linear::MICHAELCC_LINEAR_A_ADD, 
                address_reg, 
                base_addr_reg, 
                offset_reg
            ));
            return address_reg;
        }
        else {
            auto address_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                linear::MICHAELCC_LINEAR_A_ADD, 
                address_reg, 
                base_addr_reg, 
                offset_reg
            ));
            auto dest_reg = make_dest_reg(elem_size);
            m_lowerer.emit(std::make_unique<linear::load_memory>(
                dest_reg, 
                address_reg, 
                0
            ));
            return dest_reg;
        }
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::array_initializer& node) {
    auto address_reg = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
    
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(node.element_type().type());
    type_layout_calculator calculator(m_lowerer.m_platform_info);
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
    type_layout_calculator calculator(m_platform_info);
    auto elem_layout = calculator(*array_type->element_type().type());

    if (current_dimension == node.dimensions().size() - 1) { // base case for last dimension
        if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.dimensions().at(current_dimension).get())) {
            emit(std::make_unique<linear::alloca_instruction>(
                dest_reg, 
                integer_constant->value() * elem_layout.size, 
                elem_layout.alignment
            ));

            auto fill_value_reg = lower_expression(*node.fill_value());
            for (size_t i = 0; i < integer_constant->value(); i++) {
                emit(std::make_unique<linear::store_memory>(
                    dest_reg, 
                    fill_value_reg, 
                    i * elem_layout.size
                ));
            }
        }
        else {
            emit(std::make_unique<linear::valloca_instruction>(
                dest_reg, 
                lower_expression(*node.dimensions()[current_dimension]), 
                elem_layout.alignment
            ));

            emit_memset(
                dest_reg, 
                lower_expression(*node.fill_value()), 
                lower_expression(*node.dimensions()[current_dimension])
            );
        }
    }

    if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.dimensions().at(current_dimension).get())) {
        auto array_addr_reg = new_vreg(m_platform_info.pointer_size * 8);
        emit(std::make_unique<linear::alloca_instruction>(
            array_addr_reg, 
            m_platform_info.pointer_size * 8, 
            std::min<size_t>(m_platform_info.pointer_size, m_platform_info.max_alignment)
        ));

        for (size_t i = 0; i < integer_constant->value(); i++) {
            lower_allocate_array(array_addr_reg, node, current_dimension + 1);
            emit(std::make_unique<linear::store_memory>(
                array_addr_reg, 
                array_addr_reg, 
                i * m_platform_info.pointer_size
            ));
        }
    }
    else {
        throw std::runtime_error("Invalid dimension type for allocate_array (first dimension must be an integer constant).");
    }
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::allocate_array& node) {
    auto dest_reg = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.lower_allocate_array(dest_reg, node, 0);
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::struct_initializer& node) {
    auto& struct_type = node.struct_type();
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    auto struct_layout = calculator(*struct_type);

    auto dest_address = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::alloca_instruction>(
        dest_address, 
        struct_layout.size, 
        struct_layout.alignment
    ));

    m_lowerer.lower_struct_initializer(node, dest_address, 0);
    return dest_address;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::union_initializer& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    if (calculator.must_alloca(node.union_type())) {
        auto dest_address = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
        auto layout = calculator(*node.union_type());
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(
            dest_address, 
            layout.size, 
            layout.alignment
        ));

        m_lowerer.lower_union_initializer(node, dest_address, 0);
        return dest_address;
    }
    return m_lowerer.lower_expression(
        *node.initializer(), 
        m_dest_reg_use_register, 
        m_dest_reg_name
    );
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::function_call& node) {
    std::vector<linear::virtual_register> arguments;
    arguments.reserve(node.arguments().size());

    for (const auto& argument : node.arguments()) {
        auto argument_reg = m_lowerer.lower_expression(*argument);
        arguments.push_back(argument_reg);
    }

    type_layout_calculator calculator(m_lowerer.m_platform_info);
    if (calculator.must_alloca(node.get_type())) {
        throw std::runtime_error("Function return value cannot be alloca'd on the stack.");
    }
    auto dest_reg = make_dest_reg(calculator(*node.get_type().type()).size * 8);

    linear::function_call::callable callee = std::visit(overloaded{
        [this, dest_reg, &arguments](const std::shared_ptr<logic::function_definition>& function_definition) -> linear::function_call::callable {
            auto linear_function_definition = m_lowerer.m_function_definitions.at(function_definition);
            return linear_function_definition;
        },
        [this, dest_reg, &arguments](const std::unique_ptr<logic::expression>& expression) -> linear::function_call::callable {
            return m_lowerer.lower_expression(*expression);
        }
    }, node.callee());

    m_lowerer.emit(std::make_unique<linear::function_call>(
        dest_reg, 
        std::move(callee), 
        std::move(arguments)
    ));
    return dest_reg;
}

linear::virtual_register logic_lowerer::expression_lowerer::dispatch(const logic::conditional_expression& node) {
    auto condition_reg = m_lowerer.lower_expression(*node.condition());
    
    auto current_block_id = m_lowerer.current_block_id();
    auto true_block_id = m_lowerer.allocate_block_id();
    auto false_block_id = m_lowerer.allocate_block_id();

    m_lowerer.emit(std::make_unique<linear::branch_condition>(
        condition_reg, true_block_id, false_block_id, false
    ));
    m_lowerer.seal_block();

    m_lowerer.begin_block(true_block_id, { current_block_id });
    auto true_result = m_lowerer.lower_expression(*node.then_expression());
    m_lowerer.seal_block();
    
    m_lowerer.begin_block(false_block_id, { current_block_id });
    auto false_result = m_lowerer.lower_expression(*node.else_expression());
    m_lowerer.seal_block();
    
    auto final_block = m_lowerer.allocate_block_id();
    m_lowerer.begin_block(final_block, { true_block_id, false_block_id });
    auto final_result = make_dest_reg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::phi_instruction>(
        final_result, std::vector<linear::var_info>{ 
            linear::var_info{ .vreg = true_result, .block_id = true_block_id },
            linear::var_info{ .vreg = false_result, .block_id = false_block_id }
        }
    ));
    return final_result;
}