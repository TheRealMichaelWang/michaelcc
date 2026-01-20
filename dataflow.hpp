#ifndef MICHAELCC_DATAFLOW_HPP
#define MICHAELCC_DATAFLOW_HPP

#include "logical.hpp"
#include "typing.hpp"
#include <memory>

namespace michaelcc {
    namespace dataflow {
        class dataflow_pass {
        private:
            std::unique_ptr<logical_ir::expression_transformer> m_expression_pass;
            std::unique_ptr<logical_ir::statement_transformer> m_statement_pass;

            class expression_simplifier final : public logical_ir::expression_transformer {
            private:
                dataflow_pass& m_pass;

            public:
                expression_simplifier(dataflow_pass& pass) : m_pass(pass) { }

            protected:
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::integer_constant>&& node) override { return (*m_pass.m_expression_pass)(std::move(node)); }
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::floating_constant>&& node) override { return (*m_pass.m_expression_pass)(std::move(node)); }
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::string_constant>&& node) override { return (*m_pass.m_expression_pass)(std::move(node)); }
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::variable_reference>&& node) override { return (*m_pass.m_expression_pass)(std::move(node)); }
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::function_reference>&& node) override { return (*m_pass.m_expression_pass)(std::move(node)); }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::var_increment_operator>&& node) override { 
                    auto destination = std::visit(overloaded{
                        [this](const std::unique_ptr<logical_ir::expression>& destination) -> logical_ir::var_increment_operator::destination_type {
                            return (*this)(destination->clone());
                        },
                        [this](const std::shared_ptr<logical_ir::variable>& destination) -> logical_ir::var_increment_operator::destination_type {
                            return destination;
                        }
                    }, node->destination());

                    std::optional<std::unique_ptr<logical_ir::expression>> increment_amount = std::nullopt;
                    if (node->increment_amount().has_value()) {
                        increment_amount = std::make_optional((*this)(node->increment_amount().value()->clone()));
                    }

                    std::unique_ptr<logical_ir::var_increment_operator> result = std::make_unique<logical_ir::var_increment_operator>(std::move(destination), std::move(increment_amount));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::arithmetic_operator>&& node) override { 
                    std::unique_ptr<logical_ir::expression> left = (*this)(node->left()->clone());
                    std::unique_ptr<logical_ir::expression> right = (*this)(node->right()->clone());

                    auto result = std::make_unique<logical_ir::arithmetic_operator>(node->get_operator(), std::move(left), std::move(right), typing::qual_type(node->get_type()));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::unary_operation>&& node) override { 
                    std::unique_ptr<logical_ir::expression> operand = (*this)(node->operand()->clone());
                    return std::make_unique<logical_ir::unary_operation>(node->get_operator(), std::move(operand));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::type_cast>&& node) override { 
                    std::unique_ptr<logical_ir::expression> operand = (*this)(node->operand()->clone());
                    return std::make_unique<logical_ir::type_cast>(std::move(operand), typing::qual_type(node->get_type()));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::address_of>&& node) override { 
                    return std::visit(overloaded{
                        [this](const std::unique_ptr<logical_ir::array_index>& operand) -> std::unique_ptr<logical_ir::expression> {
                            return std::make_unique<logical_ir::address_of>(dynamic_unique_cast<logical_ir::array_index>((*this)(operand->clone())));
                        },
                        [this](const std::unique_ptr<logical_ir::member_access>& operand) -> std::unique_ptr<logical_ir::expression> {
                            return std::make_unique<logical_ir::address_of>(dynamic_unique_cast<logical_ir::member_access>((*this)(operand->clone())));
                        },
                        [this](const std::shared_ptr<logical_ir::variable>& operand) -> std::unique_ptr<logical_ir::expression> {
                            return std::make_unique<logical_ir::address_of>(std::shared_ptr<logical_ir::variable>(operand));
                        }
                    }, node->operand());
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::dereference>&& node) override { 
                    std::unique_ptr<logical_ir::expression> operand = (*this)(node->operand()->clone());
                    auto result = std::make_unique<logical_ir::dereference>(std::move(operand));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::member_access>&& node) override { 
                    std::unique_ptr<logical_ir::expression> base = (*this)(node->base()->clone());
                    auto result = std::make_unique<logical_ir::member_access>(std::move(base), node->member(), node->is_dereference());
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::array_index>&& node) override { 
                    std::unique_ptr<logical_ir::expression> base = (*this)(node->base()->clone());
                    std::unique_ptr<logical_ir::expression> index = (*this)(node->index()->clone());
                    auto result = std::make_unique<logical_ir::array_index>(std::move(base), std::move(index));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::array_initializer>&& node) override { 
                    std::vector<std::unique_ptr<logical_ir::expression>> initializers;
                    initializers.reserve(node->initializers().size());
                    for (const auto& initializer : node->initializers()) {
                        initializers.emplace_back((*this)(initializer->clone()));
                    }
                    auto result = std::make_unique<logical_ir::array_initializer>(std::move(initializers), typing::qual_type(node->get_type()));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::allocate_array>&& node) override { 
                    std::vector<std::unique_ptr<logical_ir::expression>> dimensions;
                    dimensions.reserve(node->dimensions().size());
                    for (const auto& dimension : node->dimensions()) {
                        dimensions.emplace_back((*this)(dimension->clone()));
                    }
                    auto result = std::make_unique<logical_ir::allocate_array>(std::move(dimensions), typing::qual_type(node->get_type()));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::struct_initializer>&& node) override { 
                    std::vector<logical_ir::struct_initializer::member_initializer> initializers;
                    initializers.reserve(node->initializers().size());
                    for (const auto& initializer : node->initializers()) {
                        initializers.emplace_back(initializer.member_name, (*this)(initializer.initializer->clone()));
                    }
                    auto result = std::make_unique<logical_ir::struct_initializer>(std::move(initializers), std::shared_ptr<typing::struct_type>(node->struct_type()));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::union_initializer>&& node) override { 
                    std::unique_ptr<logical_ir::expression> initializer = (*this)(node->initializer()->clone());
                    auto result = std::make_unique<logical_ir::union_initializer>(std::move(initializer), std::shared_ptr<typing::union_type>(node->union_type()), typing::qual_type(node->target_type()));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::function_call>&& node) override { 
                    logical_ir::function_call::callable callee = std::visit(overloaded{
                        [this](const std::shared_ptr<logical_ir::function_definition>& callee) -> logical_ir::function_call::callable {
                            return std::shared_ptr<logical_ir::function_definition>(callee);
                        },
                        [this](const std::shared_ptr<logical_ir::expression>& callee) -> logical_ir::function_call::callable {
                            return (*this)(callee->clone());
                        }
                    }, node->callee());

                    std::vector<std::unique_ptr<logical_ir::expression>> arguments;
                    arguments.reserve(node->arguments().size());
                    for (const auto& argument : node->arguments()) {
                        arguments.emplace_back((*this)(argument->clone()));
                    }
                    auto result = std::make_unique<logical_ir::function_call>(std::move(callee), std::move(arguments));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::conditional_expression>&& node) override { 
                    std::unique_ptr<logical_ir::expression> condition = (*this)(node->condition()->clone());
                    std::unique_ptr<logical_ir::expression> then_expression = (*this)(node->then_expression()->clone());
                    std::unique_ptr<logical_ir::expression> else_expression = (*this)(node->else_expression()->clone());
                    auto result = std::make_unique<logical_ir::conditional_expression>(
                        std::move(condition),
                        std::make_unique<logical_ir::type_cast>(std::move(then_expression), typing::qual_type(node->get_type())),
                        std::make_unique<logical_ir::type_cast>(std::move(else_expression), typing::qual_type(node->get_type())),
                        typing::qual_type(node->get_type())
                    );
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::set_address>&& node) override { 
                    std::unique_ptr<logical_ir::expression> destination = (*this)(node->destination()->clone());
                    std::unique_ptr<logical_ir::expression> value = (*this)(node->value()->clone());
                    auto result = std::make_unique<logical_ir::set_address>(std::move(destination), std::move(value));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::set_variable>&& node) override { 
                    std::unique_ptr<logical_ir::expression> value = (*this)(node->value()->clone());
                    auto result = std::make_unique<logical_ir::set_variable>(std::shared_ptr<logical_ir::variable>(node->variable()), std::move(value));
                    return (*m_pass.m_expression_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::enumerator_literal>&& node) override { return (*m_pass.m_expression_pass)(std::move(node)); }
            };

            class statement_traverser final: logical_ir::statement_transformer {
            private:
                dataflow_pass& m_pass;

            public:
                statement_traverser(dataflow_pass& pass) : m_pass(pass) { }

            protected:
                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::expression_statement>&& node) override { 
                    std::unique_ptr<logical_ir::expression> expression = (*m_pass.m_expression_pass)(node->get_expression()->clone());
                    auto result = std::make_unique<logical_ir::expression_statement>(std::move(expression));
                    return (*m_pass.m_statement_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::variable_declaration>&& node) override { 
                    std::shared_ptr<logical_ir::variable> variable = std::shared_ptr<logical_ir::variable>(node->get_variable());
                    std::unique_ptr<logical_ir::expression> initializer = (*m_pass.m_expression_pass)(node->initializer()->clone());
                    auto result = std::make_unique<logical_ir::variable_declaration>(std::move(variable), std::move(initializer));
                    return (*m_pass.m_statement_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::return_statement>&& node) override { 
                    std::unique_ptr<logical_ir::expression> value = (*m_pass.m_expression_pass)(node->value()->clone());
                    auto result = std::make_unique<logical_ir::return_statement>(std::move(value));
                    return (*m_pass.m_statement_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::if_statement>&& node) override { 
                    std::unique_ptr<logical_ir::expression> condition = (*m_pass.m_expression_pass)(node->condition()->clone());
                    node->then_body()->transform_statements(*m_pass.m_statement_pass);
                    if (node->else_body()) {
                        node->else_body()->transform_statements(*m_pass.m_statement_pass);
                    }
                    std::unique_ptr<logical_ir::if_statement> result = std::make_unique<logical_ir::if_statement>(
                        std::move(condition), 
                        std::shared_ptr<logical_ir::control_block>(node->then_body()), 
                        std::shared_ptr<logical_ir::control_block>(node->else_body())
                    );
                    return (*m_pass.m_statement_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::loop_statement>&& node) override { 
                    std::unique_ptr<logical_ir::expression> condition = (*m_pass.m_expression_pass)(node->condition()->clone());
                    node->body()->transform_statements(*m_pass.m_statement_pass);
                    std::unique_ptr<logical_ir::loop_statement> result = std::make_unique<logical_ir::loop_statement>(
                        std::shared_ptr<logical_ir::control_block>(node->body()),
                        std::move(condition),
                        node->check_condition_first()
                    );
                    return (*m_pass.m_statement_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::break_statement>&& node) override { 
                    auto result = std::make_unique<logical_ir::break_statement>(node->loop_depth());
                    return (*m_pass.m_statement_pass)(std::move(result));
                }

                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::continue_statement>&& node) override { 
                    auto result = std::make_unique<logical_ir::continue_statement>(node->loop_depth());
                    return (*m_pass.m_statement_pass)(std::move(result));
                }
                
                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::statement_block>&& node) override { 
                    node->control_block()->transform_statements(*m_pass.m_statement_pass);
                    auto result = std::make_unique<logical_ir::statement_block>(std::shared_ptr<logical_ir::control_block>(node->control_block()));
                    return (*m_pass.m_statement_pass)(std::move(result));
                }
            };

        public:
            dataflow_pass(std::unique_ptr<logical_ir::expression_transformer> expression_transformer, std::unique_ptr<logical_ir::statement_transformer> statement_transformer) 
                : m_expression_pass(std::move(expression_transformer)), 
                m_statement_pass(std::move(statement_transformer)) { }
        };
    }
}

#endif