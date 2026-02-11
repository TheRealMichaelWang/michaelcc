#ifndef MICHAELCC_LINEAR_FLATTEN_HPP
#define MICHAELCC_LINEAR_FLATTEN_HPP

#include "linear/ir.hpp"
#include "logic/ir.hpp"
#include "platform.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace michaelcc {
    class logic_lowerer {
    private:
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


        struct block_var_ctx {
            std::unordered_map<std::shared_ptr<logic::variable>, std::vector<linear::var_info>> m_variable_to_vreg;
        };

        struct block_builder {
            size_t id;
            std::vector<std::unique_ptr<linear::instruction>> instructions;
            std::vector<linear::var_info> incoming_vregs;
            std::vector<size_t> incoming_block_ids;
            block_var_ctx var_info;
        };

        const platform_info m_platform_info;

        std::optional<block_builder> m_current_block;
        std::unordered_map<size_t, linear::basic_block> m_finished_blocks;
        std::unordered_map<size_t, block_var_ctx> m_finished_block_var_ctx;
        size_t m_next_vreg_id = 0;
        size_t m_next_block_id = 0;

        expression_lowerer m_expression_lowerer;
        statement_lowerer m_statement_lowerer;

        linear::virtual_register new_vreg(size_t size_bits) {
            size_t id = m_next_vreg_id;
            m_next_vreg_id++;
            return { id, size_bits };
        }

        block_var_ctx reconcile_var_regs(const std::vector<size_t>& incoming_block_ids);

        linear::virtual_register get_var_reg(const std::shared_ptr<logic::variable>& variable);

        size_t begin_block(std::vector<size_t>&& incoming_block_ids) {
            size_t id = m_next_block_id;
            m_next_block_id++;
            m_current_block = block_builder{ 
                .id = id, 
                .incoming_block_ids = std::move(incoming_block_ids), 
                .var_info = reconcile_var_regs(incoming_block_ids) 
            };
            return id;
        }

        void emit(std::unique_ptr<linear::instruction>&& inst) {
            m_current_block->instructions.push_back(std::move(inst));
        }

        size_t current_block_id() const {
            return m_current_block->id;
        }

        void seal_block() {
            if (!m_current_block) return;
            auto& cb = *m_current_block;
            m_finished_blocks.emplace(cb.id, linear::basic_block(cb.id, std::move(cb.instructions)));
            m_current_block.reset();
        }

        linear::operand lower_expression(const logic::expression& expr);

        linear::virtual_register compute_lvalue_address(const logic::expression& expr);

        void lower_control_block(const logic::control_block& block);

    public:
        explicit logic_lowerer(const platform_info& platform_info);

        linear::function_definition lower_function(const logic::function_definition& func);
    };
}

#endif
