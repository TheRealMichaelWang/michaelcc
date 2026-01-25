#ifndef MICHAELCC_OPTIMIZATION_HPP
#define MICHAELCC_OPTIMIZATION_HPP

#include "logical.hpp"
#include "symbols.hpp"

#include <memory>
#include <unordered_set>
#include <functional>
#include <vector>
namespace michaelcc {
    namespace optimization {
        class transform_pass {
        public:
            class pass_mutator {
                private:
                    bool m_ir_mutated = false;
    
                public:
                    bool is_ir_mutated() const noexcept { return m_ir_mutated; }
                    void reset_mutation_flag() noexcept { m_ir_mutated = false; }

                protected:
                    void mark_ir_mutated() noexcept { m_ir_mutated = true; }
            };

            class expression_pass : public pass_mutator {
            public:
                virtual ~expression_pass() = default;

                virtual std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::integer_constant& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::floating_constant& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::string_constant& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::variable_reference>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::function_reference& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::increment_operator>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::arithmetic_operator>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::unary_operation>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::type_cast>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::address_of>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::dereference>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::member_access>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::array_index>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::array_initializer>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::allocate_array>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::struct_initializer>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::union_initializer>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::function_call>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::conditional_expression>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::set_address>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::set_variable>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::compound_expression>&& node) = 0;
                virtual std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::enumerator_literal& node) = 0;
            };

            class statement_pass : public pass_mutator {
            public:
                virtual ~statement_pass() = default;
                virtual std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::expression_statement>&& node) = 0;
                virtual std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::variable_declaration>&& node) = 0;
                virtual std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::return_statement>&& node) = 0;
                virtual std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::if_statement>&& node) = 0;
                virtual std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::loop_statement>&& node) = 0;
                virtual std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::break_statement& node) = 0;
                virtual std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::continue_statement& node) = 0;
                virtual std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::statement_block>&& node) = 0;
            };

        private:
            struct replace_variable_context {
                std::unordered_set<std::shared_ptr<logical_ir::variable>> variables_to_replace;
                std::weak_ptr<logical_ir::symbol_context> new_context;
            };

            std::unique_ptr<expression_pass> m_expression_pass;
            std::unique_ptr<statement_pass> m_statement_pass;
            std::vector<replace_variable_context> m_replace_variable_contexts;
            std::function<std::string(const std::string&)> m_variable_name_transformer;
            
            bool m_is_ir_mutated = false;
    
            class expression_traverser final : public logical_ir::expression_transformer {
            private:
                transform_pass& m_pass;

            public:
                expression_traverser(transform_pass& pass);

            protected:
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::integer_constant& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::floating_constant& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::string_constant& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::variable_reference& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::function_reference& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::increment_operator& node) override;
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
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::compound_expression& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::enumerator_literal& node) override;
            };

            class statement_traverser final : public logical_ir::statement_transformer {
            private:
                transform_pass& m_pass;

            public:
                statement_traverser(transform_pass& pass) : m_pass(pass) { }

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

            expression_traverser m_expression_traverser;
            statement_traverser m_statement_traverser;

            std::shared_ptr<logical_ir::variable> replace_variable(const std::shared_ptr<logical_ir::variable>& variable) const;
            std::shared_ptr<logical_ir::control_block> transform_control_block(const logical_ir::control_block& node);

            friend class default_statement_pass;
        public:
            transform_pass(std::unique_ptr<expression_pass>&& expression_transformer, 
                std::unique_ptr<statement_pass>&& statement_transformer, 
                std::function<std::string(const std::string&)> variable_name_transformer = [](const std::string& name) { return std::format("_{}", name); },
                std::vector<replace_variable_context> replace_variable_contexts = {});

            ~transform_pass() = default;

            void transform(logical_ir::translation_unit& unit) {
                unit.transform(m_expression_traverser, m_statement_traverser);
                m_is_ir_mutated = m_expression_pass->is_ir_mutated() || m_statement_pass->is_ir_mutated();
            }

            bool is_ir_mutated() const noexcept { return m_is_ir_mutated; }

            void reset() { 
                m_replace_variable_contexts.clear(); 
                m_is_ir_mutated = false; 
                m_expression_pass->reset_mutation_flag();
                m_statement_pass->reset_mutation_flag();
            }

            static int transform(logical_ir::translation_unit&unit, std::vector<std::unique_ptr<transform_pass>>& passes, int max_passes = 1000);
        };

        class default_expression_pass : public transform_pass::expression_pass {
        public:
            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::integer_constant& node) override { 
                return std::make_unique<logical_ir::integer_constant>(node.value(), typing::qual_type(node.get_type())); 
            }

            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::floating_constant& node) override { 
                return std::make_unique<logical_ir::floating_constant>(node.value(), typing::qual_type(node.get_type())); 
            }

            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::string_constant& node) override { 
                return std::make_unique<logical_ir::string_constant>(node.index()); 
            }
            
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::variable_reference>&& node) override { return node; }

            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::function_reference& node) override {
                return std::make_unique<logical_ir::function_reference>(std::shared_ptr<logical_ir::function_definition>(node.get_function()));
            }

            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::increment_operator>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::arithmetic_operator>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::unary_operation>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::type_cast>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::address_of>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::dereference>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::member_access>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::array_index>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::array_initializer>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::allocate_array>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::struct_initializer>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::union_initializer>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::function_call>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::conditional_expression>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::set_address>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::set_variable>&& node) override { return node; }
            std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::compound_expression>&& node) override { return node; }

            std::unique_ptr<logical_ir::expression> dispatch(const logical_ir::enumerator_literal& node) override { 
                return std::make_unique<logical_ir::enumerator_literal>(typing::enum_type::enumerator(node.enumerator()), std::shared_ptr<typing::enum_type>(node.enum_type()));
            }
        };

        class default_statement_pass : public transform_pass::statement_pass {
        public:
            std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::expression_statement>&& node) override { return node; }
            std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::variable_declaration>&& node) override { return node; }
            std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::return_statement>&& node) override { return node; }
            std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::if_statement>&& node) override { return node; }
            std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::loop_statement>&& node) override { return node; }
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::break_statement& node) override { return std::make_unique<logical_ir::break_statement>(node.loop_depth()); }
            std::unique_ptr<logical_ir::statement> dispatch(const logical_ir::continue_statement& node) override { return std::make_unique<logical_ir::continue_statement>(node.loop_depth()); }
            std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::statement_block>&& node) override { return node; }
        };
    }
}

#endif