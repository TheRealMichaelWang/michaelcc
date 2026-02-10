#include "linear/flatten.hpp"
#include "logic/type_info.hpp"
#include <bit>
#include <stdexcept>

namespace michaelcc {

    // Construction & reset

    logic_lowerer::logic_lowerer(const platform_info& platform_info)
        : m_platform_info(platform_info),
          m_layout_calculator(platform_info),
          m_expression_lowerer(*this),
          m_statement_lowerer(*this) {}

    void logic_lowerer::reset_function_state() {
        m_var_info.clear();
        m_loop_stack.clear();
        m_current_block.reset();
        m_finished_blocks.clear();
        m_compound_result_addrs.clear();
        m_next_vreg_id = 0;
        m_next_block_id = 0;
    }

    // Small helpers

    linear::virtual_register logic_lowerer::ensure_vreg(linear::operand op) {
        if (auto* vreg = std::get_if<linear::virtual_register>(&op)) {
            return *vreg;
        }
        auto& lit = std::get<linear::literal>(op);
        auto dest = new_vreg(lit.size_bits);
        emit(std::make_unique<linear::copy_instruction>(dest, op));
        return dest;
    }

    size_t logic_lowerer::type_size_bytes(const typing::qual_type& type) {
        return m_layout_calculator(*type.type()).size;
    }

    linear::operand logic_lowerer::lower_expression(const logic::expression& expr) {
        // const_expression_dispatcher::operator() takes `const expression&`.
        return m_expression_lowerer(expr);
    }

    void logic_lowerer::lower_control_block(const logic::control_block& block) {
        for (const auto& stmt : block.statements()) {
            if (!has_open_block()) break; // dead code after unconditional branch
            m_statement_lowerer(*stmt);
        }
    }

    // Returns a vreg holding the memory address of an lvalue expression.

    linear::virtual_register logic_lowerer::compute_lvalue_address(const logic::expression& expr) {
        // variable_reference: return the alloca vreg (local) or a global_address (global)
        if (auto* var_ref = dynamic_cast<const logic::variable_reference*>(&expr)) {
            auto it = m_var_info.find(var_ref->get_variable());
            if (it != m_var_info.end()) {
                return it->second;
            }
            auto dest = new_vreg(ptr_bits());
            emit(std::make_unique<linear::global_address>(dest, var_ref->get_variable()->name()));
            return dest;
        }

        // dereference: the pointer value IS the address
        if (auto* deref = dynamic_cast<const logic::dereference*>(&expr)) {
            return ensure_vreg(lower_expression(*deref->operand()));
        }

        // member_access: base address + field offset
        if (auto* member = dynamic_cast<const logic::member_access*>(&expr)) {
            linear::virtual_register base_addr;
            if (member->is_dereference()) {
                // ptr->field: base expression is a pointer
                base_addr = ensure_vreg(lower_expression(*member->base()));
            } else {
                // val.field: base is an lvalue
                base_addr = compute_lvalue_address(*member->base());
            }
            size_t offset = member->member().offset;
            if (offset == 0) return base_addr;

            auto dest = new_vreg(ptr_bits());
            emit(std::make_unique<linear::a_instruction>(
                linear::ADD, dest,
                linear::operand(base_addr),
                linear::operand(linear::literal{ offset, ptr_bits() })
            ));
            return dest;
        }

        // array_index: base address + index * element_size
        if (auto* arr_idx = dynamic_cast<const logic::array_index*>(&expr)) {
            auto base_type = arr_idx->base()->get_type();
            linear::virtual_register base_addr;

            if (base_type.is_same_type<typing::array_type>()) {
                // Array decays to its alloca address.
                base_addr = compute_lvalue_address(*arr_idx->base());
            } else {
                // Pointer: the pointer value IS the base address.
                base_addr = ensure_vreg(lower_expression(*arr_idx->base()));
            }

            size_t elem_bytes = type_size_bytes(arr_idx->get_type());
            auto idx = ensure_vreg(lower_expression(*arr_idx->index()));

            auto offset = new_vreg(ptr_bits());
            emit(std::make_unique<linear::a_instruction>(
                linear::MULTIPLY, offset,
                linear::operand(idx),
                linear::operand(linear::literal{ elem_bytes, ptr_bits() })
            ));

            auto result = new_vreg(ptr_bits());
            emit(std::make_unique<linear::a_instruction>(
                linear::ADD, result,
                linear::operand(base_addr),
                linear::operand(offset)
            ));
            return result;
        }

        throw std::runtime_error("compute_lvalue_address: expression is not an lvalue");
    }

    // Token to a_instruction_type mapping

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

    // Expression lowering

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::integer_constant& node) {
        size_t bits = m_lowerer.type_size_bits(node.get_type());
        return linear::literal{ static_cast<uint64_t>(node.value()), bits };
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::floating_constant& node) {
        size_t bits = m_lowerer.type_size_bits(node.get_type());
        return linear::literal{ std::bit_cast<uint64_t>(node.value()), bits };
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::string_constant& node) {
        // The string lives in the program's string table; return its address.
        auto dest = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        std::string sym = "__str_" + std::to_string(node.index());
        m_lowerer.emit(std::make_unique<linear::global_address>(dest, std::move(sym)));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::variable_reference& node) {
        size_t bits = m_lowerer.type_size_bits(node.get_type());

        auto it = m_lowerer.m_var_info.find(node.get_variable());
        if (it != m_lowerer.m_var_info.end()) {
            // Local variable: load from stack slot
            auto dest = m_lowerer.new_vreg(bits);
            m_lowerer.emit(std::make_unique<linear::m_instruction>(
                linear::LOAD, dest, it->second, 0));
            return dest;
        }

        // Global variable: load address then load value
        auto addr = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::global_address>(addr, node.get_variable()->name()));
        auto dest = m_lowerer.new_vreg(bits);
        m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::LOAD, dest, addr, 0));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::function_reference& node) {
        auto dest = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::global_address>(dest, node.get_function()->name()));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::increment_operator& node) {
        // destination += increment_amount  (default amount = 1)
        // Returns the NEW value.
        return std::visit([this, &node](auto&& dest_variant) -> linear::operand {
            using T = std::decay_t<decltype(dest_variant)>;

            linear::virtual_register addr;
            size_t bits;

            if constexpr (std::is_same_v<T, std::shared_ptr<logic::variable>>) {
                auto it = m_lowerer.m_var_info.find(dest_variant);
                if (it == m_lowerer.m_var_info.end())
                    throw std::runtime_error("increment_operator: variable not found");
                addr = it->second;
                bits = m_lowerer.type_size_bits(dest_variant->get_type());
            } else {
                // unique_ptr<expression>: compute lvalue address
                addr = m_lowerer.compute_lvalue_address(*dest_variant);
                bits = m_lowerer.type_size_bits(dest_variant->get_type());
            }

            // Load current value.
            auto current = m_lowerer.new_vreg(bits);
            m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::LOAD, current, addr, 0));

            // Compute increment amount.
            linear::operand inc;
            if (node.increment_amount().has_value()) {
                inc = m_lowerer.lower_expression(**node.increment_amount());
            } else {
                inc = linear::literal{ 1, bits };
            }

            // new_val = current + inc
            auto new_val = m_lowerer.new_vreg(bits);
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                linear::ADD, new_val, linear::operand(current), inc));

            // Store back.
            m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, addr, new_val, 0));

            return new_val;
        }, node.destination());
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::arithmetic_operator& node) {
        auto lhs = m_lowerer.lower_expression(*node.left());
        auto rhs = m_lowerer.lower_expression(*node.right());
        size_t bits = m_lowerer.type_size_bits(node.get_type());

        auto dest = m_lowerer.new_vreg(bits);
        m_lowerer.emit(std::make_unique<linear::a_instruction>(
            token_to_a_type(node.get_operator()), dest, lhs, rhs));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::unary_operation& node) {
        auto operand = m_lowerer.lower_expression(*node.operand());
        size_t bits = m_lowerer.type_size_bits(node.get_type());
        auto dest = m_lowerer.new_vreg(bits);

        switch (node.get_operator()) {
            case MICHAELCC_TOKEN_MINUS:
                // Negate: 0 - operand
                m_lowerer.emit(std::make_unique<linear::a_instruction>(
                    linear::SUBTRACT, dest,
                    linear::operand(linear::literal{ 0, bits }), operand));
                break;
            case MICHAELCC_TOKEN_NOT:
                m_lowerer.emit(std::make_unique<linear::a_instruction>(
                    linear::NOT, dest, operand,
                    linear::operand(linear::literal{ 0, bits })));
                break;
            case MICHAELCC_TOKEN_TILDE:
                m_lowerer.emit(std::make_unique<linear::a_instruction>(
                    linear::BITWISE_NOT, dest, operand,
                    linear::operand(linear::literal{ 0, bits })));
                break;
            default:
                throw std::runtime_error("Unsupported unary operator");
        }
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::type_cast& node) {
        auto src = m_lowerer.lower_expression(*node.operand());
        size_t target_bits = m_lowerer.type_size_bits(node.get_type());

        // Copy / reinterpret.  A later phase can insert sign/zero extension if needed.
        auto dest = m_lowerer.new_vreg(target_bits);
        m_lowerer.emit(std::make_unique<linear::copy_instruction>(dest, src));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::address_of& node) {
        return std::visit([this](auto&& op) -> linear::operand {
            using T = std::decay_t<decltype(op)>;
            if constexpr (std::is_same_v<T, std::shared_ptr<logic::variable>>) {
                auto it = m_lowerer.m_var_info.find(op);
                if (it != m_lowerer.m_var_info.end()) {
                    return it->second;
                }
                auto dest = m_lowerer.new_vreg(m_lowerer.ptr_bits());
                m_lowerer.emit(std::make_unique<linear::global_address>(dest, op->name()));
                return dest;
            } else {
                // unique_ptr<array_index> or unique_ptr<member_access>
                return m_lowerer.compute_lvalue_address(*op);
            }
        }, node.operand());
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::dereference& node) {
        auto ptr = m_lowerer.ensure_vreg(m_lowerer.lower_expression(*node.operand()));
        size_t bits = m_lowerer.type_size_bits(node.get_type());
        auto dest = m_lowerer.new_vreg(bits);
        m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::LOAD, dest, ptr, 0));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::member_access& node) {
        auto addr = m_lowerer.compute_lvalue_address(node);
        size_t bits = m_lowerer.type_size_bits(node.get_type());
        auto dest = m_lowerer.new_vreg(bits);
        m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::LOAD, dest, addr, 0));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::array_index& node) {
        auto addr = m_lowerer.compute_lvalue_address(node);
        size_t bits = m_lowerer.type_size_bits(node.get_type());
        auto dest = m_lowerer.new_vreg(bits);
        m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::LOAD, dest, addr, 0));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::array_initializer& node) {
        size_t elem_bytes = m_lowerer.type_size_bytes(node.element_type());
        size_t total_bytes = elem_bytes * node.initializers().size();

        auto base = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(base, total_bytes));

        for (size_t i = 0; i < node.initializers().size(); i++) {
            auto val = m_lowerer.ensure_vreg(m_lowerer.lower_expression(*node.initializers()[i]));
            size_t offset = i * elem_bytes;
            if (offset == 0) {
                m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, base, val, 0));
            } else {
                auto addr = m_lowerer.new_vreg(m_lowerer.ptr_bits());
                m_lowerer.emit(std::make_unique<linear::a_instruction>(
                    linear::ADD, addr,
                    linear::operand(base),
                    linear::operand(linear::literal{ offset, m_lowerer.ptr_bits() })));
                m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, addr, val, 0));
            }
        }
        return base; // pointer to the array
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::allocate_array& node) {
        // VLA: compute total byte count = product(dimensions) * elem_size,
        // then stack-allocate that many bytes.
        // NOTE: alloca_instruction currently takes a compile-time size.
        //       A proper dynamic alloca would need extending the IR.
        //       For now, fall back to a fixed-size placeholder.
        size_t elem_bytes = m_lowerer.type_size_bytes(
            typing::qual_type(std::dynamic_pointer_cast<typing::array_type>(node.get_type().type())->element_type()));

        auto total = m_lowerer.ensure_vreg(m_lowerer.lower_expression(*node.dimensions()[0]));
        for (size_t i = 1; i < node.dimensions().size(); i++) {
            auto dim = m_lowerer.ensure_vreg(m_lowerer.lower_expression(*node.dimensions()[i]));
            auto prod = m_lowerer.new_vreg(m_lowerer.ptr_bits());
            m_lowerer.emit(std::make_unique<linear::a_instruction>(
                linear::MULTIPLY, prod, linear::operand(total), linear::operand(dim)));
            total = prod;
        }

        auto byte_size = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::a_instruction>(
            linear::MULTIPLY, byte_size,
            linear::operand(total),
            linear::operand(linear::literal{ elem_bytes, m_lowerer.ptr_bits() })));

        // TODO: implement a dynamic-alloca instruction. Using a fixed 0 placeholder.
        auto dest = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(dest, 0));
        return dest;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::struct_initializer& node) {
        size_t struct_bytes = m_lowerer.type_size_bytes(node.get_type());
        auto base = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(base, struct_bytes));

        const auto& fields = node.struct_type()->fields();
        for (size_t i = 0; i < node.initializers().size(); i++) {
            auto val = m_lowerer.ensure_vreg(
                m_lowerer.lower_expression(*node.initializers()[i].initializer));

            size_t offset = fields[i].offset;
            if (offset == 0) {
                m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, base, val, 0));
            } else {
                auto addr = m_lowerer.new_vreg(m_lowerer.ptr_bits());
                m_lowerer.emit(std::make_unique<linear::a_instruction>(
                    linear::ADD, addr,
                    linear::operand(base),
                    linear::operand(linear::literal{ offset, m_lowerer.ptr_bits() })));
                m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, addr, val, 0));
            }
        }
        return base;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::union_initializer& node) {
        size_t union_bytes = m_lowerer.type_size_bytes(node.get_type());
        auto base = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(base, union_bytes));

        auto val = m_lowerer.ensure_vreg(m_lowerer.lower_expression(*node.initializer()));
        m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, base, val, 0));
        return base;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::function_call& node) {
        // Lower arguments.
        std::vector<linear::operand> args;
        args.reserve(node.arguments().size());
        for (const auto& arg : node.arguments()) {
            args.push_back(m_lowerer.lower_expression(*arg));
        }

        // Determine callee (direct name or indirect vreg).
        linear::function_call::callee_type callee;
        if (auto* func_def = std::get_if<std::shared_ptr<logic::function_definition>>(&node.callee())) {
            callee = (*func_def)->name();
        } else {
            auto& callee_expr = std::get<std::unique_ptr<logic::expression>>(node.callee());
            callee = m_lowerer.ensure_vreg(m_lowerer.lower_expression(*callee_expr));
        }

        // Destination register (none for void returns).
        std::optional<linear::virtual_register> dest;
        auto ret_type = node.get_type();
        if (!ret_type.is_same_type<typing::void_type>()) {
            dest = m_lowerer.new_vreg(m_lowerer.type_size_bits(ret_type));
        }

        m_lowerer.emit(std::make_unique<linear::function_call>(dest, std::move(callee), std::move(args)));

        return dest.has_value()
            ? linear::operand(*dest)
            : linear::operand(linear::literal{ 0, 0 }); // void dummy
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::conditional_expression& node) {
        //  cond ? then_expr : else_expr  =  branch + phi
        auto cond = m_lowerer.lower_expression(*node.condition());

        size_t then_id  = m_lowerer.new_block_id();
        size_t else_id  = m_lowerer.new_block_id();
        size_t merge_id = m_lowerer.new_block_id();

        m_lowerer.seal_with_cond_branch(cond, then_id, else_id);

        // Then branch
        m_lowerer.begin_block(then_id);
        auto then_val     = m_lowerer.lower_expression(*node.then_expression());
        size_t then_exit  = m_lowerer.current_block_id();
        m_lowerer.seal_with_branch(merge_id);

        // Else branch
        m_lowerer.begin_block(else_id);
        auto else_val     = m_lowerer.lower_expression(*node.else_expression());
        size_t else_exit  = m_lowerer.current_block_id();
        m_lowerer.seal_with_branch(merge_id);

        // Merge with phi
        m_lowerer.begin_block(merge_id);
        size_t result_bits = m_lowerer.type_size_bits(node.get_type());
        auto result = m_lowerer.new_vreg(result_bits);

        std::vector<std::pair<size_t, linear::operand>> phi_values;
        phi_values.emplace_back(then_exit, then_val);
        phi_values.emplace_back(else_exit, else_val);
        m_lowerer.emit(std::make_unique<linear::phi_instruction>(result, std::move(phi_values)));

        return result;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::set_address& node) {
        auto addr  = m_lowerer.compute_lvalue_address(*node.destination());
        auto value = m_lowerer.lower_expression(*node.value());
        auto vreg  = m_lowerer.ensure_vreg(value);

        m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, addr, vreg, 0));
        return value;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::set_variable& node) {
        auto value = m_lowerer.lower_expression(*node.value());
        auto vreg  = m_lowerer.ensure_vreg(value);

        auto it = m_lowerer.m_var_info.find(node.variable());
        if (it != m_lowerer.m_var_info.end()) {
            m_lowerer.emit(std::make_unique<linear::m_instruction>(
                linear::STORE, it->second, vreg, 0));
        } else {
            // Global variable.
            auto addr = m_lowerer.new_vreg(m_lowerer.ptr_bits());
            m_lowerer.emit(std::make_unique<linear::global_address>(addr, node.variable()->name()));
            m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, addr, vreg, 0));
        }
        return value;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::compound_expression& node) {
        // Compound expression  ({ stmts...; return expr; })
        // Allocate a slot for the result, push it, lower the block, pop & load.
        size_t bits  = m_lowerer.type_size_bits(node.get_type());
        size_t bytes = bits / 8;

        auto result_addr = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(result_addr, bytes));

        m_lowerer.m_compound_result_addrs.push_back(result_addr);
        m_lowerer.lower_control_block(*node.control_block());
        m_lowerer.m_compound_result_addrs.pop_back();

        auto result = m_lowerer.new_vreg(bits);
        m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::LOAD, result, result_addr, 0));
        return result;
    }

    linear::operand logic_lowerer::expression_lowerer::dispatch(const logic::enumerator_literal& node) {
        size_t bits = m_lowerer.type_size_bits(node.get_type());
        return linear::literal{ static_cast<uint64_t>(node.enumerator().value), bits };
    }

    // Statement lowering

    void logic_lowerer::statement_lowerer::dispatch(const logic::expression_statement& node) {
        // Evaluate for side effects, discard result.
        m_lowerer.lower_expression(*node.expression());
    }

    void logic_lowerer::statement_lowerer::dispatch(const logic::variable_declaration& node) {
        size_t sz = m_lowerer.type_size_bytes(node.variable()->get_type());

        // Allocate stack slot.
        auto alloc = m_lowerer.new_vreg(m_lowerer.ptr_bits());
        m_lowerer.emit(std::make_unique<linear::alloca_instruction>(alloc, sz));
        m_lowerer.m_var_info[node.variable()] = alloc;

        // Optional initializer.
        if (node.initializer()) {
            auto val = m_lowerer.ensure_vreg(m_lowerer.lower_expression(*node.initializer()));
            m_lowerer.emit(std::make_unique<linear::m_instruction>(linear::STORE, alloc, val, 0));
        }
    }

    void logic_lowerer::statement_lowerer::dispatch(const logic::return_statement& node) {
        // Compound-expression return: store into the compound-result slot, don't
        // emit a real function_return or break control flow.
        if (node.is_compound_return() && !m_lowerer.m_compound_result_addrs.empty()) {
            if (node.value()) {
                auto val = m_lowerer.ensure_vreg(m_lowerer.lower_expression(*node.value()));
                m_lowerer.emit(std::make_unique<linear::m_instruction>(
                    linear::STORE, m_lowerer.m_compound_result_addrs.back(), val, 0));
            }
            return; // control flow continues normally after the compound expression body
        }

        // Real function return.
        if (node.value()) {
            auto val = m_lowerer.lower_expression(*node.value());
            m_lowerer.emit(std::make_unique<linear::function_return>(std::optional<linear::operand>(val)));
        } else {
            m_lowerer.emit(std::make_unique<linear::function_return>(std::nullopt));
        }
        m_lowerer.seal_block();

        // Open a dead block for any unreachable code that follows.
        m_lowerer.begin_block(m_lowerer.new_block_id());
    }

    void logic_lowerer::statement_lowerer::dispatch(const logic::if_statement& node) {
        auto cond = m_lowerer.lower_expression(*node.condition());

        size_t then_id  = m_lowerer.new_block_id();
        size_t merge_id = m_lowerer.new_block_id();

        if (node.else_body()) {
            size_t else_id = m_lowerer.new_block_id();
            m_lowerer.seal_with_cond_branch(cond, then_id, else_id);

            // Then
            m_lowerer.begin_block(then_id);
            m_lowerer.lower_control_block(*node.then_body());
            if (m_lowerer.has_open_block()) m_lowerer.seal_with_branch(merge_id);

            // Else
            m_lowerer.begin_block(else_id);
            m_lowerer.lower_control_block(*node.else_body());
            if (m_lowerer.has_open_block()) m_lowerer.seal_with_branch(merge_id);
        } else {
            m_lowerer.seal_with_cond_branch(cond, then_id, merge_id);

            // Then
            m_lowerer.begin_block(then_id);
            m_lowerer.lower_control_block(*node.then_body());
            if (m_lowerer.has_open_block()) m_lowerer.seal_with_branch(merge_id);
        }

        // Merge: subsequent statements emit here
        m_lowerer.begin_block(merge_id);
    }

    void logic_lowerer::statement_lowerer::dispatch(const logic::loop_statement& node) {
        if (node.check_condition_first()) {
            //  while (cond) { body }
            //
            //  current -> branch -> header
            //  header:  cond -> body / exit
            //  body:    ... -> branch -> header
            //  exit:    (subsequent code)

            size_t header_id = m_lowerer.new_block_id();
            size_t body_id   = m_lowerer.new_block_id();
            size_t exit_id   = m_lowerer.new_block_id();

            m_lowerer.seal_with_branch(header_id);

            // Header: evaluate condition
            m_lowerer.begin_block(header_id);
            auto cond = m_lowerer.lower_expression(*node.condition());
            m_lowerer.seal_with_cond_branch(cond, body_id, exit_id);

            // Body
            m_lowerer.m_loop_stack.push_back({ .continue_target = header_id, .break_target = exit_id });
            m_lowerer.begin_block(body_id);
            m_lowerer.lower_control_block(*node.body());
            if (m_lowerer.has_open_block()) m_lowerer.seal_with_branch(header_id);
            m_lowerer.m_loop_stack.pop_back();

            // Exit
            m_lowerer.begin_block(exit_id);

        } else {
            //  do { body } while (cond)
            //
            //  current -> branch -> body
            //  body:    ... -> branch -> cond_block
            //  cond_block: cond -> body / exit
            //  exit:    (subsequent code)

            size_t body_id = m_lowerer.new_block_id();
            size_t cond_id = m_lowerer.new_block_id();
            size_t exit_id = m_lowerer.new_block_id();

            m_lowerer.seal_with_branch(body_id);

            // Body
            m_lowerer.m_loop_stack.push_back({ .continue_target = cond_id, .break_target = exit_id });
            m_lowerer.begin_block(body_id);
            m_lowerer.lower_control_block(*node.body());
            if (m_lowerer.has_open_block()) m_lowerer.seal_with_branch(cond_id);
            m_lowerer.m_loop_stack.pop_back();

            // Condition
            m_lowerer.begin_block(cond_id);
            auto cond = m_lowerer.lower_expression(*node.condition());
            m_lowerer.seal_with_cond_branch(cond, body_id, exit_id);

            // Exit
            m_lowerer.begin_block(exit_id);
        }
    }

    void logic_lowerer::statement_lowerer::dispatch(const logic::break_statement& node) {
        size_t depth = static_cast<size_t>(node.loop_depth());
        size_t idx   = m_lowerer.m_loop_stack.size() - 1 - depth;
        m_lowerer.seal_with_branch(m_lowerer.m_loop_stack[idx].break_target);

        // Dead block for any unreachable code after break.
        m_lowerer.begin_block(m_lowerer.new_block_id());
    }

    void logic_lowerer::statement_lowerer::dispatch(const logic::continue_statement& node) {
        size_t depth = static_cast<size_t>(node.loop_depth());
        size_t idx   = m_lowerer.m_loop_stack.size() - 1 - depth;
        m_lowerer.seal_with_branch(m_lowerer.m_loop_stack[idx].continue_target);

        // Dead block for any unreachable code after continue.
        m_lowerer.begin_block(m_lowerer.new_block_id());
    }

    void logic_lowerer::statement_lowerer::dispatch(const logic::statement_block& node) {
        // Nested scope: just lower its contents in the current block
        m_lowerer.lower_control_block(*node.control_block());
    }

    // Public API

    linear::function_definition logic_lowerer::lower_function(const logic::function_definition& func) {
        reset_function_state();

        size_t entry_id = new_block_id();
        begin_block(entry_id);

        // Materialise each parameter into a vreg, then spill into a stack slot
        // so that subsequent code can treat it like any other local variable.
        for (size_t i = 0; i < func.parameters().size(); i++) {
            const auto& param = func.parameters()[i];
            size_t sz   = type_size_bytes(param->get_type());
            size_t bits = sz * 8;

            auto param_vreg = new_vreg(bits);
            emit(std::make_unique<linear::function_param>(param_vreg, i));

            auto alloc_vreg = new_vreg(ptr_bits());
            emit(std::make_unique<linear::alloca_instruction>(alloc_vreg, sz));
            emit(std::make_unique<linear::m_instruction>(linear::STORE, alloc_vreg, param_vreg, 0));

            m_var_info[param] = alloc_vreg;
        }

        // Lower the function body (function_definition extends control_block).
        lower_control_block(func);

        // If the block is still open (no explicit return at the end), emit
        // an implicit void return.
        if (has_open_block()) {
            emit(std::make_unique<linear::function_return>(std::nullopt));
            seal_block();
        }

        return linear::function_definition(
            func.name(), entry_id, std::move(m_finished_blocks), m_next_vreg_id);
    }

    linear::program logic_lowerer::lower(const logic::translation_unit& unit) {
        linear::program result;
        result.string_constants = unit.strings();

        for (const auto& sym : unit.global_symbols()) {
            auto* func = dynamic_cast<logic::function_definition*>(sym.get());
            if (func && func->is_implemented()) {
                result.functions.push_back(lower_function(*func));
            }
        }

        return result;
    }

} // namespace michaelcc
