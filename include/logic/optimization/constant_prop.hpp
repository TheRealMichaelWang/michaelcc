#ifndef MICHAELCC_CONSTANT_PROPAGATION
#define MICHAELCC_CONSTANT_PROPAGATION

//#include "logic/optimization.hpp"
#include "logic/optimization/ir_simplify.hpp"
#include "logic/optimization.hpp"
#include "logic/ir.hpp"
#include "logic/typing.hpp"
#include "platform.hpp"
#include <memory>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace michaelcc {
    namespace optimization {
        class variable_use_analyzer : public logic::const_visitor{
        public:
            struct variable_metrics {
                bool is_mutated = false;
                int num_uses = 0;
            };
            class mutated_variable_tracker : public logic::const_expression_dispatcher<void> {
            private:
                std::unordered_set<std::shared_ptr<logic::variable>> m_mutated_variables;
                std::unordered_set<const logic::expression*> m_dead_expressions;

            public:
                std::tuple<
                    std::unordered_set<std::shared_ptr<logic::variable>>, 
                    std::unordered_set<const logic::expression*>
                > get_mutated_variables(const logic::expression& expression) { 
                    m_mutated_variables = std::unordered_set<std::shared_ptr<logic::variable>>();
                    m_dead_expressions = std::unordered_set<const logic::expression*>();
                    (*this)(expression);
                    return std::make_tuple(std::move(m_mutated_variables), std::move(m_dead_expressions));
                }

            protected:
                void dispatch(const logic::variable_reference& node) override {
                    m_mutated_variables.insert(node.get_variable());
                    m_dead_expressions.insert(&node);
                }

                void dispatch(const logic::array_index& node) override {
                    (*this)(*node.base());
                }

                void dispatch(const logic::member_access& node) override {
                    (*this)(*node.base());
                }

                void dispatch(const logic::compound_expression& node) override {
                    (*this)(*node.return_expression());
                }

                void dispatch(const logic::type_cast& node) override {
                    (*this)(*node.operand());
                }
                
                void dispatch(const logic::dereference& node) override {
                    (*this)(*node.operand());
                }

                void dispatch(const logic::address_of& node) override {
                    std::visit(overloaded{
                        [&](const std::shared_ptr<logic::variable>& variable) {
                            m_mutated_variables.insert(variable);
                        },
                        [&](const std::unique_ptr<logic::array_index>& array_index) {
                            (*this)(*array_index);
                        },
                        [&](const std::unique_ptr<logic::member_access>& member_access) {
                            (*this)(*member_access);
                        },
                    }, node.operand());
                }

                void handle_default(const logic::expression& node) override { }
            };

        private:

            std::unordered_set<const logic::expression*> m_dead_expressions;
            std::unordered_map<std::shared_ptr<logic::variable>, variable_metrics> m_variable_metrics;
            std::unordered_set<const logic::expression*> m_mutated_expressions;

        public:
            std::unordered_map<std::shared_ptr<logic::variable>, variable_metrics> get_metrics(logic::translation_unit& program) {
                m_variable_metrics = std::unordered_map<std::shared_ptr<logic::variable>, variable_metrics>();
                program.accept(*this);
                return std::move(m_variable_metrics); 
            }

        protected:
            void visit(const logic::variable_declaration& node) override;
            void visit(const logic::set_variable& node) override;
            void visit(const logic::set_address& node) override;
            void visit(const logic::increment_operator& node) override;
            void visit(const logic::variable_reference& node) override;
            void visit(const logic::address_of& node) override;
            void visit(const logic::function_definition& node) override;
            void visit(const logic::function_call& node) override;
            void visit(const logic::expression_statement& node) override;
        };

        class constant_cloner final : public logic::const_expression_dispatcher<std::unique_ptr<logic::expression>> {
        private:
            const platform_info& m_platform_info;

        public:
            constant_cloner(const platform_info& platform_info) : m_platform_info(platform_info) {}

        protected:
            std::unique_ptr<logic::expression> dispatch(const logic::integer_constant& node) override { 
                return std::make_unique<logic::integer_constant>(node.value(), typing::qual_type(node.get_type())); 
            }

            std::unique_ptr<logic::expression> dispatch(const logic::floating_constant& node) override { 
                return std::make_unique<logic::floating_constant>(node.value(), typing::qual_type(node.get_type())); 
            }

            std::unique_ptr<logic::expression> dispatch(const logic::string_constant& node) override { 
                return std::make_unique<logic::string_constant>(node.index()); 
            }

            std::unique_ptr<logic::expression> dispatch(const logic::enumerator_literal& node) override { 
                return std::make_unique<logic::enumerator_literal>(typing::enum_type::enumerator(node.enumerator()), std::shared_ptr<typing::enum_type>(node.enum_type())); 
            }

            std::unique_ptr<logic::expression> dispatch(const logic::array_initializer& node) override { 
                std::vector<std::unique_ptr<logic::expression>> initializers;
                initializers.reserve(node.initializers().size());
                for (const auto& initializer : node.initializers()) {
                    auto element = (*this)(*initializer);
                    if (!element || !node.element_type().is_assignable_from(element->get_type(), m_platform_info)) {
                        return nullptr;
                    }

                    initializers.emplace_back(std::move(element));
                }
                return std::make_unique<logic::array_initializer>(std::move(initializers), typing::qual_type(node.element_type()));
            }
            
            std::unique_ptr<logic::expression> dispatch(const logic::struct_initializer& node) override { 
                std::vector<logic::struct_initializer::member_initializer> initializers;
                initializers.reserve(node.initializers().size());
                for (size_t i = 0; i < node.initializers().size(); i++) {
                    auto element = (*this)(*node.initializers()[i].initializer);
                    if (!element || !node.struct_type()->fields()[i].member_type.is_assignable_from(element->get_type(), m_platform_info)) {
                        return nullptr;
                    }

                    initializers.emplace_back(logic::struct_initializer::member_initializer{node.initializers()[i].member_name, std::move(element)});
                }
                return std::make_unique<logic::struct_initializer>(std::move(initializers), std::shared_ptr<typing::struct_type>(node.struct_type()));
            }

            std::unique_ptr<logic::expression> dispatch(const logic::union_initializer& node) override { 
                auto element = (*this)(*node.initializer());
                if (!element || !node.target_member().member_type.is_assignable_from(element->get_type(), m_platform_info)) {
                    return nullptr;
                }
                return std::make_unique<logic::union_initializer>(std::move(element), std::shared_ptr<typing::union_type>(node.union_type()), typing::member(node.target_member()));
            }

            std::unique_ptr<logic::expression> dispatch(const logic::type_cast& node) override { 
                auto expression = (*this)(*node.operand());
                if (!expression) {
                    return nullptr;
                }
                return std::make_unique<logic::type_cast>(std::move(expression), typing::qual_type(node.get_type()));
            }

            std::unique_ptr<logic::expression> handle_default(const logic::expression& node) override { return nullptr; }
        };

        class constant_prop_pass final : public default_pass {
        private:
            class dest_side_effects : public logic::expression_dispatcher<void> {
            private:
                std::vector<std::unique_ptr<logic::statement>> m_statements;

            public:
                std::vector<std::unique_ptr<logic::statement>> get_statements(logic::expression& expression) {
                    m_statements = std::vector<std::unique_ptr<logic::statement>>();
                    (*this)(expression);
                    std::reverse(m_statements.begin(), m_statements.end());
                    return std::move(m_statements);
                }

            protected:
                void dispatch(logic::array_index& node) override {
                    m_statements.emplace_back(std::make_unique<logic::expression_statement>(node.release_index()));
                    (*this)(*node.base());
                }

                void dispatch(logic::compound_expression& node) override {
                    std::vector<std::unique_ptr<logic::statement>> statements = node.control_block()->release_statements();
                    (*this)(*node.return_expression());
                    m_statements.insert(m_statements.end(), 
                        std::make_move_iterator(statements.rbegin()), 
                        std::make_move_iterator(statements.rend()));
                }

                void dispatch(logic::member_access& node) override {
                    (*this)(*node.base());
                }

                void dispatch(logic::address_of& node) override {
                    std::visit(overloaded{
                        [&](const std::shared_ptr<logic::variable>& variable) { },
                        [&](const std::unique_ptr<logic::array_index>& array_index) {
                            (*this)(*array_index);
                        },
                        [&](const std::unique_ptr<logic::member_access>& member_access) {
                            (*this)(*member_access);
                        },
                    }, node.operand());
                }

                void dispatch(logic::type_cast& node) override {
                    (*this)(*node.operand());
                }
                
                void dispatch(logic::dereference& node) override {
                    (*this)(*node.operand());
                }

                void handle_default(logic::expression& node) override { }
            };

            class expression_pass : public default_expression_pass {
            private:
                constant_prop_pass& m_pass;

            public:
                expression_pass(constant_prop_pass& pass) : m_pass(pass) {}

                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_variable>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_address>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::increment_operator>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::variable_reference>&& node) override;
            };

            class statement_pass : public default_statement_pass {
            private:
                constant_prop_pass& m_pass;

            public:
                statement_pass(constant_prop_pass& pass) : m_pass(pass) {}

                std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::variable_declaration>&& node) override;
            };

            std::unordered_map<std::shared_ptr<logic::variable>, variable_use_analyzer::variable_metrics> m_variable_metrics;
            std::unordered_map<std::shared_ptr<logic::variable>, std::unique_ptr<logic::expression>> m_constant_expressions;
            const platform_info m_platform_info;

        protected:
            bool can_propagate_constant(const std::shared_ptr<logic::variable>& variable) const {
                return m_variable_metrics.at(variable).is_mutated == false 
                    && !(variable->get_type().qualifiers() & typing::VOLATILE_TYPE_QUALIFIER);
            }

            bool can_remove_variable(const std::shared_ptr<logic::variable>& variable) const {
                return m_variable_metrics.at(variable).num_uses == 0
                && !(variable->get_type().qualifiers() & typing::VOLATILE_TYPE_QUALIFIER);
            }

            std::unique_ptr<logic::expression> propagate_constant(const std::shared_ptr<logic::variable>& variable) const {
                if (!can_propagate_constant(variable) || !m_constant_expressions.contains(variable)) {
                    return nullptr;
                }
                constant_cloner cloner(m_platform_info);
                return cloner(*m_constant_expressions.at(variable));
            }

        public:
            constant_prop_pass(logic::translation_unit& program, const platform_info& platform_info) : default_pass(
                std::make_unique<expression_pass>(*this),
                std::make_unique<statement_pass>(*this),
                [](const std::string& name) { return name; }
            ), m_variable_metrics(std::make_unique<variable_use_analyzer>()->get_metrics(program)), m_platform_info(platform_info) {}
        };

        std::unique_ptr<pass> inline make_constant_prop_pass(logic::translation_unit& program, const platform_info& platform_info) {
            std::vector<std::unique_ptr<pass>> passes;
            passes.reserve(2);
            passes.emplace_back(std::make_unique<ir_simplify_pass>());
            passes.emplace_back(std::make_unique<constant_prop_pass>(program, platform_info));
            return std::make_unique<compound_pass>(std::move(passes));
        }
    }
}
#endif