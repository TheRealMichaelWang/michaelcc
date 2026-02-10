#ifndef MICHAELCC_LINEAR_FLATTEN_HPP
#define MICHAELCC_LINEAR_FLATTEN_HPP

#include "linear/ir.hpp"
#include "logic/ir.hpp"
#include "logic/type_info.hpp"
#include "platform.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace michaelcc {
    class logic_lowerer {
    private:
        // Expression dispatcher
        class expression_lowerer : public logic::const_expression_dispatcher<linear::operand> {
        private:
            logic_lowerer& m_lowerer;

        public:
            expression_lowerer(logic_lowerer& lowerer) : m_lowerer(lowerer) {}

        protected:
            linear::operand dispatch(const logic::integer_constant& node) override;
            linear::operand dispatch(const logic::floating_constant& node) override;
            linear::operand dispatch(const logic::string_constant& node) override;
            linear::operand dispatch(const logic::variable_reference& node) override;
            linear::operand dispatch(const logic::function_reference& node) override;
            linear::operand dispatch(const logic::increment_operator& node) override;
            linear::operand dispatch(const logic::arithmetic_operator& node) override;
            linear::operand dispatch(const logic::unary_operation& node) override;
            linear::operand dispatch(const logic::type_cast& node) override;
            linear::operand dispatch(const logic::address_of& node) override;
            linear::operand dispatch(const logic::dereference& node) override;
            linear::operand dispatch(const logic::member_access& node) override;
            linear::operand dispatch(const logic::array_index& node) override;
            linear::operand dispatch(const logic::array_initializer& node) override;
            linear::operand dispatch(const logic::allocate_array& node) override;
            linear::operand dispatch(const logic::struct_initializer& node) override;
            linear::operand dispatch(const logic::union_initializer& node) override;
            linear::operand dispatch(const logic::function_call& node) override;
            linear::operand dispatch(const logic::conditional_expression& node) override;
            linear::operand dispatch(const logic::set_address& node) override;
            linear::operand dispatch(const logic::set_variable& node) override;
            linear::operand dispatch(const logic::compound_expression& node) override;
            linear::operand dispatch(const logic::enumerator_literal& node) override;            
        };

        // Statement dispatcher
        class statement_lowerer : public logic::const_statement_dispatcher<void> {
        private:
            logic_lowerer& m_lowerer;

        public:
            statement_lowerer(logic_lowerer& lowerer) : m_lowerer(lowerer) {}

        protected:
            void dispatch(const logic::expression_statement& node) override;
            void dispatch(const logic::variable_declaration& node) override;
            void dispatch(const logic::return_statement& node) override;
            void dispatch(const logic::if_statement& node) override;
            void dispatch(const logic::loop_statement& node) override;
            void dispatch(const logic::break_statement& node) override;
            void dispatch(const logic::continue_statement& node) override;
            void dispatch(const logic::statement_block& node) override;
        };

        // Maps each local variable to the vreg holding its stack slot address.
        using var_map = std::unordered_map<std::shared_ptr<logic::variable>, linear::virtual_register>;

        // Loop context for break / continue
        struct loop_context {
            size_t continue_target; // block to branch to on continue
            size_t break_target;    // block to branch to on break
        };

        // Block under construction
        struct block_builder {
            size_t id;
            std::vector<std::unique_ptr<linear::instruction>> instructions;
        };

        // Lowerer state
        const platform_info m_platform_info;
        type_layout_calculator m_layout_calculator;

        size_t m_next_vreg_id = 0;
        size_t m_next_block_id = 0;

        // Per-function
        var_map m_var_info;
        std::vector<loop_context> m_loop_stack;
        std::optional<block_builder> m_current_block;
        std::unordered_map<size_t, linear::basic_block> m_finished_blocks;

        // Compound-expression result propagation (stack for nesting)
        std::vector<linear::virtual_register> m_compound_result_addrs;

        // Dispatchers
        expression_lowerer m_expression_lowerer;
        statement_lowerer m_statement_lowerer;

        // Helpers

        linear::virtual_register new_vreg(size_t size_bits) {
            return { m_next_vreg_id++, size_bits };
        }

        size_t new_block_id() {
            return m_next_block_id++;
        }

        void begin_block(size_t id) {
            m_current_block = block_builder{ .id = id };
        }

        void emit(std::unique_ptr<linear::instruction>&& inst) {
            m_current_block->instructions.push_back(std::move(inst));
        }

        size_t current_block_id() const {
            return m_current_block->id;
        }

        bool has_open_block() const {
            return m_current_block.has_value();
        }

        void seal_block() {
            if (!m_current_block) return;
            auto& cb = *m_current_block;
            m_finished_blocks.emplace(cb.id, linear::basic_block(cb.id, std::move(cb.instructions)));
            m_current_block.reset();
        }

        // Seal the current block with an unconditional branch to target.
        void seal_with_branch(size_t target) {
            emit(std::make_unique<linear::branch>(target));
            seal_block();
        }

        // Seal the current block with a conditional branch.
        void seal_with_cond_branch(linear::operand cond, size_t true_target, size_t false_target) {
            emit(std::make_unique<linear::branch_condition>(cond, true_target, false_target));
            seal_block();
        }

        // Ensure an operand is a virtual register; copies a literal into a vreg if needed.
        linear::virtual_register ensure_vreg(linear::operand op);

        // Get the byte size of a type via the layout calculator.
        size_t type_size_bytes(const typing::qual_type& type);

        // Convenience: bit size = bytes * 8.
        size_t type_size_bits(const typing::qual_type& type) {
            return type_size_bytes(type) * 8;
        }

        // Pointer width in bits for this platform.
        size_t ptr_bits() const {
            return m_platform_info.pointer_size * 8;
        }

        // Lower an expression (delegates to the expression dispatcher).
        linear::operand lower_expression(const logic::expression& expr);

        // Compute the memory address of an lvalue expression (variable, deref, member, array-index).
        linear::virtual_register compute_lvalue_address(const logic::expression& expr);

        // Lower every statement inside a control block.
        void lower_control_block(const logic::control_block& block);

        // Reset all per-function state.
        void reset_function_state();

    public:
        explicit logic_lowerer(const platform_info& platform_info);

        // Lower a single function definition into a linear function.
        linear::function_definition lower_function(const logic::function_definition& func);

        // Lower an entire translation unit into a linear program.
        linear::program lower(const logic::program& unit);
    };
}

#endif
