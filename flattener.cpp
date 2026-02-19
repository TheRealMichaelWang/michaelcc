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
        auto dest_reg = new_vreg(calculator(*variable->get_type().type()).size * 8);
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
    auto dest_reg = new_vreg(calculator(*variable->get_type().type()).size * 8);
    
    emit(std::make_unique<linear::phi_instruction>(dest_reg, std::move(vregs)));

    // update the current block's var info
    m_current_block->var_info.m_variable_to_vreg[variable] = { linear::var_info{ .vreg = dest_reg, .block_id = current_block_id() } };

    return dest_reg;
}

void logic_lowerer::emit_iloop(linear::operand count, std::function<void(linear::virtual_register)> body) {
    auto initial_state = new_vreg(m_platform_info.int_size * 8);
    emit(std::make_unique<linear::init_zero>(initial_state));

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
        linear::COMPARE_EQUAL, cond_check_status, iterator_state, count
    ));

    emit(std::make_unique<linear::branch_condition>(
        cond_check_status, finish_block_id, loop2_block_id, true
    ));
    seal_block();

    begin_block(loop2_block_id, { loop1_block_id });

    body(iterator_state);

    //increment and branch back to the top
    emit(std::make_unique<linear::a2_instruction>(
        linear::ADD, post_increment_state, iterator_state, 1
    ));
    emit(std::make_unique<linear::branch>(loop1_block_id));
    seal_block();

    begin_block(finish_block_id, { loop1_block_id });
}

void logic_lowerer::emit_memset(linear::virtual_register dest, linear::operand value, linear::operand count) {
    emit_iloop(count, [this, dest, value](linear::virtual_register iterator_state) {
        auto offset_reg = new_vreg(m_platform_info.int_size * 8);
        emit(std::make_unique<linear::a2_instruction>(
            linear::MULTIPLY, offset_reg, iterator_state, 
            linear::get_operand_size(value) / 8
        ));
        auto address_reg = new_vreg(m_platform_info.pointer_size * 8);
        emit(std::make_unique<linear::a_instruction>(
            linear::ADD, address_reg, dest, offset_reg
        ));
        emit(std::make_unique<linear::store_memory>(address_reg, value, 0));
    });
}

void logic_lowerer::emit_memcpy(linear::virtual_register dest, linear::operand src, size_t size_bytes, size_t offset) {
    for (size_t i = 0; i < size_bytes; i += m_platform_info.pointer_size) {
        auto elem_reg = new_vreg(m_platform_info.char_size);    
        emit(std::make_unique<linear::load_memory>(elem_reg, src, i + offset));
        emit(std::make_unique<linear::store_memory>(dest, elem_reg, i + offset));
    }
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
    type_layout_calculator calculator(m_lowerer.m_platform_info);

    if (variable->must_alloca()) {
        type_layout_calculator calculator(m_lowerer.m_platform_info);
        if (calculator(*variable->get_type().type()).size <= m_lowerer.m_platform_info.pointer_size) {
            auto dest_reg = m_lowerer.new_vreg(calculator(*variable->get_type().type()).size * 8);

            m_lowerer.emit(std::make_unique<linear::load_memory>(
                dest_reg, 
                var_reg, 
                0
            ));

            return dest_reg;
        }
        else {
            if (!variable->get_type().is_same_type<typing::struct_type>()) {
                throw std::runtime_error("Only structs can be this large and passed by value.");
            }
        }
    }
    return var_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::function_reference& node) {
    auto dest_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::load_effective_address>(dest_reg, node.get_function()->name()));
    return dest_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::increment_operator& node) {
    bool use_one = node.get_operator() == MICHAELCC_TOKEN_INCREMENT || node.get_operator() == MICHAELCC_TOKEN_DECREMENT;
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

    if (std::holds_alternative<std::shared_ptr<logic::variable>>(node.destination())) {
        auto variable = std::get<std::shared_ptr<logic::variable>>(node.destination());
        if (!variable->must_alloca()) {
            auto var_reg = m_lowerer.get_var_reg(variable);

            auto new_dest_reg = m_lowerer.new_vreg(var_reg.size_bits);
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                type, 
                new_dest_reg, 
                var_reg, 
                use_one ? linear::literal{ .value = 1, .size_bits = var_reg.size_bits } 
                    : m_lowerer.lower_expression(*node.increment_amount().value())
            ));
            
            m_lowerer.m_current_block->var_info.m_variable_to_vreg[variable] = { linear::var_info{ .vreg = new_dest_reg, .block_id = m_lowerer.current_block_id() } };
            return new_dest_reg;
        }
    }

    type_layout_calculator calculator(m_lowerer.m_platform_info);
    size_t var_size = calculator(*node.get_type().type()).size * 8;

    auto dest_reg = std::visit(overloaded{
        [this](const std::unique_ptr<logic::expression>& expr) -> linear::virtual_register {
            return m_lowerer.compute_lvalue_address(*expr);
        },
        [this](const std::shared_ptr<logic::variable>& variable) -> linear::virtual_register {
            return m_lowerer.get_var_reg(variable);
        }
    }, node.destination());

    auto result_reg = m_lowerer.new_vreg(var_size);
    m_lowerer.emit(std::make_unique<linear::load_memory>(
        result_reg, 
        dest_reg, 
        0 // offset
    ));
    auto incremented_reg = m_lowerer.new_vreg(var_size);
    m_lowerer.emit(std::make_unique<linear::a_instruction>(
        type, 
        incremented_reg, 
        result_reg, 
        use_one ? linear::literal{ .value = 1, .size_bits = var_size } 
            : m_lowerer.lower_expression(*node.increment_amount().value())
    ));
    m_lowerer.emit(std::make_unique<linear::store_memory>(
        dest_reg, 
        incremented_reg, 
        0 // offset
    ));
    return result_reg;
}

static linear::a_instruction_type token_to_a_type(token_type op) {
    switch (op) {
        case MICHAELCC_TOKEN_PLUS:           return linear::ADD;
        case MICHAELCC_TOKEN_MINUS:          return linear::SUBTRACT;
        case MICHAELCC_TOKEN_ASTERISK:       return linear::MULTIPLY;
        case MICHAELCC_TOKEN_SLASH:          return linear::DIVIDE;
        case MICHAELCC_TOKEN_MODULO:         return linear::MODULO;
        case MICHAELCC_TOKEN_BITSHIFT_LEFT:  return linear::SHIFT_LEFT;
        case MICHAELCC_TOKEN_BITSHIFT_RIGHT: return linear::SHIFT_RIGHT;
        case MICHAELCC_TOKEN_AND:            return linear::BITWISE_AND;
        case MICHAELCC_TOKEN_OR:             return linear::BITWISE_OR;
        case MICHAELCC_TOKEN_CARET:          return linear::BITWISE_XOR;
        case MICHAELCC_TOKEN_DOUBLE_AND:     return linear::AND;
        case MICHAELCC_TOKEN_DOUBLE_OR:      return linear::OR;
        case MICHAELCC_TOKEN_EQUALS:         return linear::COMPARE_EQUAL;
        case MICHAELCC_TOKEN_NOT_EQUALS:     return linear::COMPARE_NOT_EQUAL;
        case MICHAELCC_TOKEN_LESS:           return linear::COMPARE_LESS_THAN;
        case MICHAELCC_TOKEN_LESS_EQUAL:     return linear::COMPARE_LESS_THAN_OR_EQUAL;
        case MICHAELCC_TOKEN_MORE:           return linear::COMPARE_GREATER_THAN;
        case MICHAELCC_TOKEN_MORE_EQUAL:     return linear::COMPARE_GREATER_THAN_OR_EQUAL;
        default: throw std::runtime_error("Unsupported binary operator for a_instruction");
    }
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::arithmetic_operator& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    size_t result_size = calculator(*node.get_type().type()).size * 8;

    linear::operand left = m_lowerer.lower_expression(*node.left());
    linear::operand right = m_lowerer.lower_expression(*node.right());

    linear::a_instruction_type type = token_to_a_type(node.get_operator());
    auto result_reg = m_lowerer.new_vreg(result_size);
    m_lowerer.emit(std::make_unique<linear::a_instruction>(type, result_reg, left, right));
    return result_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::unary_operation& node) {
    auto operand = m_lowerer.lower_expression(*node.operand());
    type_layout_calculator calculator(m_lowerer.m_platform_info);

    size_t bits = calculator(*node.get_type().type()).size * 8;
    auto dest = m_lowerer.new_vreg(bits);

    switch (node.get_operator()) {
        case MICHAELCC_TOKEN_MINUS:
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                linear::SUBTRACT, dest,
                linear::operand(linear::literal{ 0, bits }),
                operand
            ));
            break;
        case MICHAELCC_TOKEN_NOT:
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                linear::NOT, dest, operand,
                linear::operand(linear::literal{ 0, bits })
            ));
            break;
        case MICHAELCC_TOKEN_TILDE:
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                linear::BITWISE_NOT, dest, operand,
                linear::operand(linear::literal{ 0, bits })
            ));
            break;
        default:
            throw std::runtime_error("Unsupported unary operator");
    }
    return dest;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::type_cast& node) {
    return m_lowerer.lower_expression(*node.operand());
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::address_of& node) {
    return std::visit(overloaded{
        [this](const std::unique_ptr<logic::array_index>& operand) -> linear::operand {
            return m_lowerer.compute_lvalue_address(*operand);
        },
        [this](const std::unique_ptr<logic::member_access>& operand) -> linear::operand {
            return m_lowerer.compute_lvalue_address(*operand);
        },
        [this](const std::shared_ptr<logic::variable>& operand) -> linear::operand {
            if (!operand->must_alloca()) {
                throw std::runtime_error("Address of variable must be alloca'd on the stack.");
            }
            return m_lowerer.get_var_reg(operand);
        }
    }, node.operand());
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::dereference& node) {
    if (node.operand()->get_type().is_same_type<typing::struct_type>()) { //we only pass around pointers to structs
        return m_lowerer.lower_expression(*node.operand());
    }

    type_layout_calculator calculator(m_lowerer.m_platform_info);
    size_t object_size = calculator(*node.operand()->get_type().type()).size * 8;

    auto operand = m_lowerer.lower_expression(*node.operand());
    auto dest_reg = m_lowerer.new_vreg(object_size);
    m_lowerer.emit(std::make_unique<linear::load_memory>(
        dest_reg, 
        operand, 
        0 // offset
    ));
    return dest_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::member_access& node) {
    if (node.member().member_type.is_same_type<typing::struct_type>()) { //return a pointer to start of the struct
        auto dest_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
        m_lowerer.emit(std::make_unique<linear::a2_instruction>(
            linear::ADD, 
            dest_reg, 
            m_lowerer.lower_expression(*node.base()), 
            node.member().offset
        ));
        return dest_reg;
    }

    type_layout_calculator calculator(m_lowerer.m_platform_info);
    size_t object_size = calculator(*node.member().member_type.type()).size * 8;

    auto object_reg = m_lowerer.lower_expression(*node.base());
    auto dest_reg = m_lowerer.new_vreg(object_size);
    m_lowerer.emit(std::make_unique<linear::load_memory>(
        dest_reg, 
        object_reg, 
        node.member().offset
    ));
    return dest_reg;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::array_index& node) {
    type_layout_calculator calculator(m_lowerer.m_platform_info);
 
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(node.base()->get_type().type());

    size_t elem_size = calculator(*array_type->element_type().type()).size * 8;
    auto offset_reg = m_lowerer.new_vreg(elem_size);
    m_lowerer.emit(std::make_unique<linear::a2_instruction>(
        linear::MULTIPLY, 
        offset_reg, 
        m_lowerer.lower_expression(*node.index()), 
        elem_size
    ));

    if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.index().get())) {
        if (array_type->element_type().is_same_type<typing::struct_type>()) {
            auto address_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
            m_lowerer.emit(std::make_unique<linear::a2_instruction>(
                linear::ADD, 
                address_reg, 
                m_lowerer.lower_expression(*node.base()), 
                integer_constant->value() * elem_size
            ));
            return address_reg;
        }
        
        auto dest_reg = m_lowerer.new_vreg(elem_size);
        m_lowerer.emit(std::make_unique<linear::load_memory>(
            dest_reg, 
            m_lowerer.lower_expression(*node.base()), 
            integer_constant->value() * elem_size
        ));
        return dest_reg;
    } else {
        auto address_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
        m_lowerer.emit(std::make_unique<linear::a_instruction>(
            linear::ADD, 
            address_reg, 
            m_lowerer.lower_expression(*node.base()), 
            offset_reg
        ));
        
        if (array_type->element_type().is_same_type<typing::struct_type>()) {
            return address_reg;
        }

        auto dest_reg = m_lowerer.new_vreg(elem_size);
        m_lowerer.emit(std::make_unique<linear::load_memory>(
            dest_reg, 
            address_reg, 
            0
        ));
        return dest_reg;
    }
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::array_initializer& node) {
    auto address_reg = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
    
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(node.element_type().type());
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    auto elem_layout = calculator(*array_type->element_type().type());
    
    m_lowerer.emit(std::make_unique<linear::alloca_instruction>(
        address_reg, 
        node.initializers().size() * elem_layout.size,
        elem_layout.alignment
    ));

    for (size_t i = 0; i < node.initializers().size(); i++) {
        if (node.initializers().at(i)->get_type().is_same_type<typing::struct_type>()) {
            if (auto struct_initializer = dynamic_cast<logic::struct_initializer*>(node.initializers().at(i).get())) {
                m_lowerer.lower_struct_initializer(*struct_initializer, address_reg, elem_layout.size * i);
            }
            else {
                m_lowerer.emit_memcpy(
                    address_reg, 
                    m_lowerer.lower_expression(*node.initializers()[i]), 
                    elem_layout.size,
                    elem_layout.size * i
                );
            }
        }
        else {
            m_lowerer.emit(std::make_unique<linear::store_memory>(
            address_reg, 
                m_lowerer.lower_expression(*node.initializers()[i]), 
                i * elem_layout.size
            ));
        }
    }
    return address_reg;
}

linear::operand logic_lowerer::lower_allocate_array(const logic::allocate_array& node, size_t current_dimension) {
    auto address_reg = new_vreg(m_platform_info.pointer_size * 8);
   
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(node.get_type().type());
    type_layout_calculator calculator(m_platform_info);
    auto elem_layout = calculator(*array_type->element_type().type());

    if (current_dimension == node.dimensions().size() - 1) { // base case for last dimension
        if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.dimensions().at(current_dimension).get())) {
            emit(std::make_unique<linear::alloca_instruction>(
                address_reg, 
                integer_constant->value() * elem_layout.size, 
                elem_layout.alignment
            ));

            auto fill_value = lower_expression(*node.fill_value());
            for (size_t i = 0; i < integer_constant->value(); i++) {
                emit(std::make_unique<linear::store_memory>(
                    address_reg, 
                    fill_value, 
                    i * elem_layout.size
                ));
            }
        }
        else {
            emit(std::make_unique<linear::valloca_instruction>(
                address_reg, 
                lower_expression(*node.dimensions()[current_dimension]), 
                elem_layout.alignment
            ));

            emit_memset(
                address_reg, 
                lower_expression(*node.fill_value()), 
                lower_expression(*node.dimensions()[current_dimension])
            );
        }
        return address_reg;
    }

    if (auto integer_constant = dynamic_cast<logic::integer_constant*>(node.dimensions().at(current_dimension).get())) {
        emit(std::make_unique<linear::alloca_instruction>(
            address_reg, 
            m_platform_info.pointer_size * 8, 
            std::min<size_t>(m_platform_info.pointer_size, m_platform_info.max_alignment)
        ));

        for (size_t i = 0; i < integer_constant->value(); i++) {
            emit(std::make_unique<linear::store_memory>(
                address_reg, 
                lower_allocate_array(node, current_dimension + 1), 
                i * m_platform_info.pointer_size
            ));
        }
        return address_reg;
    }
    else {
        throw std::runtime_error("Invalid dimension type for allocate_array (first dimension must be an integer constant).");
    }
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::allocate_array& node) {
    return m_lowerer.lower_allocate_array(node, 0);
}


linear::operand logic_lowerer::lower_struct_initializer(const logic::struct_initializer& node, linear::virtual_register dest_address, size_t offset) {
    auto& struct_type = node.struct_type();
    
    type_layout_calculator calculator(m_platform_info);
    for (const auto& initializer : node.initializers()) {
        auto field = std::find_if(struct_type->fields().begin(), struct_type->fields().end(), [&](const typing::member& member) {
            return member.name == initializer.member_name;
        });
        if (field == struct_type->fields().end()) {
            throw std::runtime_error("Member \"" + initializer.member_name + "\" not found in struct.");
        }

        if (initializer.initializer->get_type().is_same_type<typing::struct_type>()) {
            if (auto struct_initializer = dynamic_cast<logic::struct_initializer*>(initializer.initializer.get())) {
                lower_struct_initializer(*struct_initializer, dest_address, field->offset + offset);
            }
            else {
                auto evaluated_src = lower_expression(*initializer.initializer);
                if (std::holds_alternative<linear::virtual_register>(evaluated_src)) {
                    auto src_address = std::get<linear::virtual_register>(evaluated_src);
                    emit_memcpy(
                        dest_address, 
                        src_address, 
                        calculator(*field->member_type.type()).size, 
                        field->offset + offset
                    );
                }
                else {
                    throw std::runtime_error("Invalid source type for struct initializer.");
                }
            }
        }
        else {
            emit(std::make_unique<linear::store_memory>(
                dest_address, 
                lower_expression(*initializer.initializer), 
                field->offset + offset
            ));
        }
    }
    return dest_address;
}

linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::struct_initializer& node) {
    auto& struct_type = node.struct_type();
    type_layout_calculator calculator(m_lowerer.m_platform_info);
    auto struct_layout = calculator(*struct_type);

    auto dest_address = m_lowerer.new_vreg(m_lowerer.m_platform_info.pointer_size * 8);
    m_lowerer.emit(std::make_unique<linear::alloca_instruction>(
        dest_address, 
        struct_layout.size, 
        struct_layout.alignment
    ));

    m_lowerer.lower_struct_initializer(node, dest_address, 0);
    return dest_address;
}