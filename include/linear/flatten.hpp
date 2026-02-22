#ifndef MICHAELCC_LINEAR_FLATTEN_HPP
#define MICHAELCC_LINEAR_FLATTEN_HPP

#include "linear/ir.hpp"
#include "logic/ir.hpp"
#include "platform.hpp"
#include "linear/registers.hpp"
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <functional>

namespace michaelcc {
    class logic_lowerer {
    private:
        class expression_lowerer : public logic::const_expression_dispatcher<linear::virtual_register> {
        private:
            logic_lowerer& m_lowerer;

        public:
            expression_lowerer(logic_lowerer& lowerer) : m_lowerer(lowerer) {}

        protected:
            linear::virtual_register dispatch(const logic::integer_constant& node) override;
            linear::virtual_register dispatch(const logic::floating_constant& node) override;
            linear::virtual_register dispatch(const logic::string_constant& node) override;
            linear::virtual_register dispatch(const logic::enumerator_literal& node) override;  
            linear::virtual_register dispatch(const logic::variable_reference& node) override;
            linear::virtual_register dispatch(const logic::set_address& node) override;
            linear::virtual_register dispatch(const logic::set_variable& node) override;
            linear::virtual_register dispatch(const logic::function_reference& node) override;
            linear::virtual_register dispatch(const logic::increment_operator& node) override;
            linear::virtual_register dispatch(const logic::arithmetic_operator& node) override;
            linear::virtual_register dispatch(const logic::unary_operation& node) override;
            linear::virtual_register dispatch(const logic::type_cast& node) override;
            linear::virtual_register dispatch(const logic::address_of& node) override;
            linear::virtual_register dispatch(const logic::dereference& node) override;
            linear::virtual_register dispatch(const logic::member_access& node) override;
            linear::virtual_register dispatch(const logic::array_index& node) override;
            linear::virtual_register dispatch(const logic::array_initializer& node) override;
            linear::virtual_register dispatch(const logic::allocate_array& node) override;
            linear::virtual_register dispatch(const logic::struct_initializer& node) override;
            linear::virtual_register dispatch(const logic::union_initializer& node) override;
            linear::virtual_register dispatch(const logic::function_call& node) override;
            linear::virtual_register dispatch(const logic::conditional_expression& node) override;
            linear::virtual_register dispatch(const logic::compound_expression& node) override;          
        };

        class lvalue_lowerer : public logic::const_expression_dispatcher<linear::virtual_register> {
        private:
            logic_lowerer& m_lowerer;

        public:
            lvalue_lowerer(logic_lowerer& lowerer) : m_lowerer(lowerer) {}

        protected:
            linear::virtual_register dispatch(const logic::variable_reference& node) override;
            linear::virtual_register dispatch(const logic::array_index& node) override;
            linear::virtual_register dispatch(const logic::member_access& node) override;

            linear::virtual_register handle_default(const logic::expression& expr) override {
                throw std::runtime_error("Cannot lower lvalue expression.");
            }
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

            block_var_ctx var_info;
        };

        struct loop_info {
            size_t block_id;
            size_t finish_block_id;
            std::optional<size_t> alternate_continue_target_block_id = std::nullopt;

            std::vector<size_t> incoming_break_block_ids;
            std::vector<size_t> incoming_continue_block_ids;
            std::unordered_map<std::shared_ptr<logic::variable>, std::vector<linear::var_info>> original_var_info;
            std::unordered_map<std::shared_ptr<logic::variable>, linear::phi_instruction*> init_phi_nodes;
        };

        struct compound_expression_info {
            size_t return_block_id;
            std::vector<size_t> incoming_block_ids;
            std::vector<linear::var_info> return_var_info;
        };

        const platform_info m_platform_info;

        std::optional<block_builder> m_current_block;
        std::unordered_map<size_t, linear::basic_block> m_finished_blocks;
        std::unordered_map<size_t, block_var_ctx> m_finished_block_var_ctx;
        std::unordered_map<size_t, loop_info> m_loop_infos;
        std::vector<linear::function_definition> m_function_definitions;
        std::unordered_map<size_t, std::shared_ptr<linear::alloc_information>> m_vreg_alloc_information;
        std::vector<size_t> m_loop_stack;
        std::vector<compound_expression_info> m_compound_expression_stack;
        size_t m_next_vreg_id = 0;
        size_t m_next_block_id = 0;

        linear::virtual_register new_vreg(size_t size_bits) {
            size_t id = m_next_vreg_id++;
            return { id, size_bits };
        }

        block_var_ctx reconcile_var_regs(const std::vector<size_t>& incoming_block_ids);

        void emit_phi_all();

        void recurse_block(size_t source_block_id, size_t target_block_id);

        linear::virtual_register get_var_reg(const std::shared_ptr<logic::variable>& variable);

        size_t allocate_block_id() {
            size_t id = m_next_block_id;
            m_next_block_id++;
            return id;
        }

        void begin_block(size_t head_block_id, const std::vector<size_t> incoming_block_ids, bool is_loop_start=false) {
            m_current_block = block_builder{ 
                .id = head_block_id, 
                .var_info = reconcile_var_regs(incoming_block_ids) 
            };
            if (is_loop_start) {
                emit_phi_all();
            }
        }

        void emit(std::unique_ptr<linear::instruction>&& inst) {
            m_current_block->instructions.push_back(std::move(inst));
        }

        size_t current_block_id() const {
            return m_current_block->id;
        }

        size_t current_loop_depth() const {
            return m_loop_stack.size();
        }

        void pop_loop_stack() {
            m_loop_stack.pop_back();
        }

        size_t seal_block() {
            if (!m_current_block) throw std::runtime_error("Cannot seal a block that does not exist");
            auto& cb = *m_current_block;
            m_finished_blocks.emplace(cb.id, linear::basic_block(cb.id, std::move(cb.instructions)));
            m_finished_block_var_ctx.emplace(cb.id, std::move(cb.var_info));
            size_t return_id = cb.id;
            m_current_block.reset();
            return return_id;
        }

        void emit_iloop(linear::virtual_register count, std::function<void(linear::virtual_register)> body);

        void emit_memset(linear::virtual_register dest, linear::virtual_register value, linear::virtual_register count);
        void emit_memcpy(linear::virtual_register dest, linear::virtual_register src, size_t size_bytes, size_t offset);

        void lower_allocate_array(linear::virtual_register dest_reg,const logic::allocate_array& node, size_t current_dimension);
        void lower_struct_initializer(const logic::struct_initializer& node, linear::virtual_register dest_address, size_t offset);
        void lower_union_initializer(const logic::union_initializer& node, linear::virtual_register dest_address, size_t offset);

        std::optional<size_t> lower_statements(const std::vector<std::unique_ptr<logic::statement>>& statements);

        linear::virtual_register lower_at_address(linear::virtual_register dest_address, const std::unique_ptr<logic::expression>& initializer, size_t offset);

        linear::virtual_register lower_expression(const logic::expression& expr) {
            auto lowerer = std::make_unique<expression_lowerer>(*this);
            return (*lowerer)(expr);
        }

        void lower_statement(const logic::statement& statement) {
            auto lowerer = std::make_unique<statement_lowerer>(*this);
            (*lowerer)(statement);
        }

        linear::virtual_register compute_lvalue_address(const logic::expression& expr) {
            auto lowerer = std::make_unique<lvalue_lowerer>(*this);
            return (*lowerer)(expr);
        }

        void lower_function(const logic::function_definition& function);
    public:
        explicit logic_lowerer(const platform_info& platform_info);
    };
}

#endif
