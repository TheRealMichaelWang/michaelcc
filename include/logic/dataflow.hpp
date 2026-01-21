#ifndef MICHAELCC_DATAFLOW_HPP
#define MICHAELCC_DATAFLOW_HPP

#include "logical.hpp"
#include <memory>
#include <unordered_set>
#include <functional>
#include "symbols.hpp"

namespace michaelcc {
    namespace dataflow {
        class transform_pass {
        private:
            struct replace_variable_context {
                std::unordered_set<std::shared_ptr<logical_ir::variable>> variables_to_replace;
                std::weak_ptr<logical_ir::symbol_context> new_context;
            };

            std::unique_ptr<logical_ir::expression_transformer> m_expression_pass;
            std::unique_ptr<logical_ir::statement_transformer> m_statement_pass;
            std::vector<replace_variable_context> m_replace_variable_contexts;
            std::function<std::string(const std::string&)> m_variable_name_transformer;
    
            class expression_simplifier final : public logical_ir::expression_transformer {
            private:
                transform_pass& m_pass;

            public:
                expression_simplifier(transform_pass& pass);

            protected:
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::integer_constant& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::floating_constant& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::string_constant& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::variable_reference& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::function_reference& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::var_increment_operator& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::arithmetic_operator& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::unary_operation& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::type_cast& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::address_of& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::dereference& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::member_access& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::array_index& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::array_initializer& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::allocate_array& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::struct_initializer& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::union_initializer& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::function_call& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::conditional_expression& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::set_address& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::set_variable& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::enumerator_literal& node) override;
            };

            class statement_traverser final: logical_ir::statement_transformer {
            private:
                transform_pass& m_pass;

            public:
                statement_traverser(transform_pass& pass);

            protected:;
                std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::expression_statement& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::variable_declaration& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::return_statement& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::if_statement& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::loop_statement& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::break_statement& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::continue_statement& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::statement_block& node) override;
            };

            std::shared_ptr<logical_ir::variable> replace_variable(const std::shared_ptr<logical_ir::variable>& variable) const;
            std::shared_ptr<logical_ir::control_block> transform_control_block(const logical_ir::control_block& node);

            friend class default_statement_transformer;
        public:
            transform_pass(std::unique_ptr<logical_ir::expression_transformer> expression_transformer, 
                std::unique_ptr<logical_ir::statement_transformer> statement_transformer, 
                std::function<std::string(const std::string&)> variable_name_transformer = [](const std::string& name) { return std::format("_{}", name); },
                std::vector<replace_variable_context> replace_variable_contexts = {});

            void transform(const logical_ir::translation_unit& unit);
        };

        class default_expression_transformer : public logical_ir::expression_transformer {
        protected:
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::integer_constant& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::floating_constant& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::string_constant& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::variable_reference& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::function_reference& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::var_increment_operator& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::arithmetic_operator& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::unary_operation& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::type_cast& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::address_of& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::dereference& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::member_access& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::array_index& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::array_initializer& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::allocate_array& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::struct_initializer& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::union_initializer& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::function_call& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::conditional_expression& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::set_address& node) override {return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::set_variable& node) override { return node.clone(); }
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::enumerator_literal& node) override { return node.clone(); }
        };

        class default_statement_transformer : public logical_ir::statement_transformer {
        private:
            transform_pass& m_pass;
        public:
            default_statement_transformer(transform_pass& pass) : m_pass(pass) { }
        protected:
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::expression_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::variable_declaration& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::return_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::if_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::loop_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::break_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::continue_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::statement_block& node) override;
        };
    }
}

#endif