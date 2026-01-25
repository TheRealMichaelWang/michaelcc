#ifndef MICHAELCC_OPTIMIZATION_HPP
#define MICHAELCC_OPTIMIZATION_HPP

#include "ir.hpp"
#include "symbols.hpp"

#include <memory>
#include <unordered_set>
#include <functional>
#include <vector>
namespace michaelcc {
    namespace optimization {
        class pass {
        public:
            virtual ~pass() = default;
            virtual void transform(logic::translation_unit& unit) = 0;
            virtual bool is_ir_mutated() const noexcept = 0;
            virtual void reset() = 0;
        };

        int transform(logic::translation_unit&unit, std::vector<std::unique_ptr<pass>>& passes, int max_passes = 1000);

        class default_pass : public pass {
        private:
            class pass_mutator {
                private:
                    bool m_ir_mutated = false;
    
                public:
                    bool is_ir_mutated() const noexcept { return m_ir_mutated; }
                    void reset_mutation_flag() noexcept { m_ir_mutated = false; }

                protected:
                    void mark_ir_mutated() noexcept { m_ir_mutated = true; }
            };
        public:
            class expression_pass : public pass_mutator {
            public:
                virtual ~expression_pass() = default;

                virtual std::unique_ptr<logic::expression> dispatch(const logic::integer_constant& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(const logic::floating_constant& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(const logic::string_constant& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::variable_reference>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(const logic::function_reference& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::increment_operator>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::arithmetic_operator>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::unary_operation>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::type_cast>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::address_of>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::dereference>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::member_access>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::array_index>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::array_initializer>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::allocate_array>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::struct_initializer>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::union_initializer>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::function_call>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::conditional_expression>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_address>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_variable>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::compound_expression>&& node) = 0;
                virtual std::unique_ptr<logic::expression> dispatch(const logic::enumerator_literal& node) = 0;
            };

            class statement_pass : public pass_mutator {
            public:
                virtual ~statement_pass() = default;
                virtual std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::expression_statement>&& node) = 0;
                virtual std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::variable_declaration>&& node) = 0;
                virtual std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::return_statement>&& node) = 0;
                virtual std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::if_statement>&& node) = 0;
                virtual std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::loop_statement>&& node) = 0;
                virtual std::unique_ptr<logic::statement> dispatch(const logic::break_statement& node) = 0;
                virtual std::unique_ptr<logic::statement> dispatch(const logic::continue_statement& node) = 0;
                virtual std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::statement_block>&& node) = 0;
            };

        private:
            struct replace_variable_context {
                std::unordered_set<std::shared_ptr<logic::variable>> variables_to_replace;
                std::weak_ptr<logic::symbol_context> new_context;
            };

            std::unique_ptr<expression_pass> m_expression_pass;
            std::unique_ptr<statement_pass> m_statement_pass;
            std::vector<replace_variable_context> m_replace_variable_contexts;
            std::function<std::string(const std::string&)> m_variable_name_transformer;
            
            bool m_is_ir_mutated = false;
    
            class expression_traverser final : public logic::expression_transformer {
            private:
                default_pass& m_pass;

            public:
                expression_traverser(default_pass& pass);

            protected:
                std::unique_ptr<logic::expression> dispatch(const logic::integer_constant& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::floating_constant& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::string_constant& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::variable_reference& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::function_reference& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::increment_operator& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::arithmetic_operator& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::unary_operation& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::type_cast& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::address_of& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::dereference& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::member_access& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::array_index& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::array_initializer& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::allocate_array& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::struct_initializer& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::union_initializer& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::function_call& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::conditional_expression& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::set_address& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::set_variable& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::compound_expression& node) override;
                std::unique_ptr<logic::expression> dispatch(const logic::enumerator_literal& node) override;
            };

            class statement_traverser final : public logic::statement_transformer {
            private:
                default_pass& m_pass;

            public:
                statement_traverser(default_pass& pass) : m_pass(pass) { }

            protected:
                std::unique_ptr<logic::statement> dispatch(const logic::expression_statement& node) override;
                std::unique_ptr<logic::statement> dispatch(const logic::variable_declaration& node) override;
                std::unique_ptr<logic::statement> dispatch(const logic::return_statement& node) override;
                std::unique_ptr<logic::statement> dispatch(const logic::if_statement& node) override;
                std::unique_ptr<logic::statement> dispatch(const logic::loop_statement& node) override;
                std::unique_ptr<logic::statement> dispatch(const logic::break_statement& node) override;
                std::unique_ptr<logic::statement> dispatch(const logic::continue_statement& node) override;
                std::unique_ptr<logic::statement> dispatch(const logic::statement_block& node) override;
            };

            expression_traverser m_expression_traverser;
            statement_traverser m_statement_traverser;

            std::shared_ptr<logic::variable> replace_variable(const std::shared_ptr<logic::variable>& variable) const;
            std::shared_ptr<logic::control_block> transform_control_block(const logic::control_block& node);

            friend class default_statement_pass;
        public:
            default_pass(std::unique_ptr<expression_pass>&& expression_transformer, 
                std::unique_ptr<statement_pass>&& statement_transformer, 
                std::function<std::string(const std::string&)> variable_name_transformer = [](const std::string& name) { return std::format("_{}", name); },
                std::vector<replace_variable_context> replace_variable_contexts = {});

            ~default_pass() = default;

            void transform(logic::translation_unit& unit) {
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
        };

        class default_expression_pass : public default_pass::expression_pass {
        public:
            std::unique_ptr<logic::expression> dispatch(const logic::integer_constant& node) override { 
                return std::make_unique<logic::integer_constant>(node.value(), typing::qual_type(node.get_type())); 
            }

            std::unique_ptr<logic::expression> dispatch(const logic::floating_constant& node) override { 
                return std::make_unique<logic::floating_constant>(node.value(), typing::qual_type(node.get_type())); 
            }

            std::unique_ptr<logic::expression> dispatch(const logic::string_constant& node) override { 
                return std::make_unique<logic::string_constant>(node.index()); 
            }
            
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::variable_reference>&& node) override { return node; }

            std::unique_ptr<logic::expression> dispatch(const logic::function_reference& node) override {
                return std::make_unique<logic::function_reference>(std::shared_ptr<logic::function_definition>(node.get_function()));
            }

            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::increment_operator>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::arithmetic_operator>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::unary_operation>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::type_cast>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::address_of>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::dereference>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::member_access>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::array_index>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::array_initializer>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::allocate_array>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::struct_initializer>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::union_initializer>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::function_call>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::conditional_expression>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_address>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_variable>&& node) override { return node; }
            std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::compound_expression>&& node) override { return node; }

            std::unique_ptr<logic::expression> dispatch(const logic::enumerator_literal& node) override { 
                return std::make_unique<logic::enumerator_literal>(typing::enum_type::enumerator(node.enumerator()), std::shared_ptr<typing::enum_type>(node.enum_type()));
            }
        };

        class default_statement_pass : public default_pass::statement_pass {
        public:
            std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::expression_statement>&& node) override { return node; }
            std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::variable_declaration>&& node) override { return node; }
            std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::return_statement>&& node) override { return node; }
            std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::if_statement>&& node) override { return node; }
            std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::loop_statement>&& node) override { return node; }
            std::unique_ptr<logic::statement> dispatch(const logic::break_statement& node) override { return std::make_unique<logic::break_statement>(node.loop_depth()); }
            std::unique_ptr<logic::statement> dispatch(const logic::continue_statement& node) override { return std::make_unique<logic::continue_statement>(node.loop_depth()); }
            std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::statement_block>&& node) override { return node; }
        };
    }
}

#endif