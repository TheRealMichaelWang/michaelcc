#include "logic/optimization.hpp"
#include "logic/ir.hpp"
#include "logic/typing.hpp"
#include "logic/ir.hpp"
#include <memory>

namespace michaelcc {
    namespace logic {
        namespace optimization {
            // expression_traverser constructor
            default_pass::expression_traverser::expression_traverser(default_pass& pass) : m_pass(pass) { }

            // expression_traverser dispatch methods
            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::integer_constant& node) {
                return m_pass.m_expression_pass->dispatch(node);
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::floating_constant& node) {
                return m_pass.m_expression_pass->dispatch(node);
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::string_constant& node) {
                return m_pass.m_expression_pass->dispatch(node);
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::variable_reference& node) {
                std::shared_ptr<logic::variable> variable = m_pass.replace_variable(node.get_variable());
                auto to_transform = std::make_unique<logic::variable_reference>(std::move(variable));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::function_reference& node) {
                return m_pass.m_expression_pass->dispatch(node);
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::increment_operator& node) {
                auto destination = std::visit(overloaded{
                    [this](const std::unique_ptr<logic::expression>& destination) -> logic::increment_operator::destination_type {
                        return (*this)(*destination);
                    },
                    [this](const std::shared_ptr<logic::variable>& destination) -> logic::increment_operator::destination_type {
                        return m_pass.replace_variable(destination);
                    }
                }, node.destination());

                std::optional<std::unique_ptr<logic::expression>> increment_amount = std::nullopt;
                if (node.increment_amount().has_value()) {
                    increment_amount = std::make_optional((*this)(*(node.increment_amount().value())));
                }

                auto to_transform = std::make_unique<logic::increment_operator>(node.get_operator(), std::move(destination), std::move(increment_amount));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::arithmetic_operator& node) {
                std::unique_ptr<logic::expression> left = (*this)(*node.left());
                std::unique_ptr<logic::expression> right = (*this)(*node.right());

                auto to_transform = std::make_unique<logic::arithmetic_operator>(node.get_operator(), std::move(left), std::move(right), typing::qual_type(node.get_type()));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::unary_operation& node) {
                std::unique_ptr<logic::expression> operand = (*this)(*node.operand());
                auto to_transform = std::make_unique<logic::unary_operation>(node.get_operator(), std::move(operand));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::type_cast& node) {
                std::unique_ptr<logic::expression> operand = (*this)(*node.operand());
                auto to_transform = std::make_unique<logic::type_cast>(std::move(operand), typing::qual_type(node.get_type()));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::address_of& node) {
                std::unique_ptr<logic::address_of> to_transform = std::visit(overloaded{
                    [this](const std::unique_ptr<logic::array_index>& operand) -> std::unique_ptr<logic::address_of> {
                        auto transformed_operand = dynamic_unique_cast<logic::array_index>((*this)(*operand));
                        if (transformed_operand == nullptr) {
                            throw std::runtime_error("Failed to transform array index");
                        }
                        return std::make_unique<logic::address_of>(std::move(transformed_operand));
                    },
                    [this](const std::unique_ptr<logic::member_access>& operand) -> std::unique_ptr<logic::address_of> {
                        auto transformed_operand = dynamic_unique_cast<logic::member_access>((*this)(*operand));
                        if (transformed_operand == nullptr) {
                            throw std::runtime_error("Failed to transform member access");
                        }
                        return std::make_unique<logic::address_of>(std::move(transformed_operand));
                    },
                    [this](const std::shared_ptr<logic::variable>& operand) -> std::unique_ptr<logic::address_of> {
                        return std::make_unique<logic::address_of>(m_pass.replace_variable(operand));
                    }
                }, node.operand());
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::dereference& node) {
                std::unique_ptr<logic::expression> operand = (*this)(*node.operand());
                auto to_transform = std::make_unique<logic::dereference>(std::move(operand));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::member_access& node) {
                std::unique_ptr<logic::expression> base = (*this)(*node.base());
                auto to_transform = std::make_unique<logic::member_access>(std::move(base), node.member(), node.is_dereference());
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::array_index& node) {
                std::unique_ptr<logic::expression> base = (*this)(*node.base());
                std::unique_ptr<logic::expression> index = (*this)(*node.index());
                auto to_transform = std::make_unique<logic::array_index>(std::move(base), std::move(index));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::array_initializer& node) {
                std::vector<std::unique_ptr<logic::expression>> initializers;
                initializers.reserve(node.initializers().size());
                for (const auto& initializer : node.initializers()) {
                    initializers.emplace_back((*this)(*initializer));
                }
                auto to_transform = std::make_unique<logic::array_initializer>(std::move(initializers), typing::qual_type(node.element_type()));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::allocate_array& node) {
                std::vector<std::unique_ptr<logic::expression>> dimensions;
                dimensions.reserve(node.dimensions().size());
                for (const auto& dimension : node.dimensions()) {
                    dimensions.emplace_back((*this)(*dimension));
                }
                std::unique_ptr<logic::expression> fill_value = (*this)(*node.fill_value());
                auto to_transform = std::make_unique<logic::allocate_array>(std::move(dimensions), std::move(fill_value), typing::qual_type(node.get_type()));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::struct_initializer& node) {
                std::vector<logic::struct_initializer::member_initializer> initializers;
                initializers.reserve(node.initializers().size());
                for (const auto& initializer : node.initializers()) {
                    initializers.emplace_back(initializer.member_name, (*this)(*initializer.initializer));
                }
                auto to_transform = std::make_unique<logic::struct_initializer>(std::move(initializers), std::shared_ptr<typing::struct_type>(node.struct_type()));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::union_initializer& node) {
                std::unique_ptr<logic::expression> initializer = (*this)(*node.initializer());
                auto to_transform = std::make_unique<logic::union_initializer>(std::move(initializer), std::shared_ptr<typing::union_type>(node.union_type()), typing::member(node.target_member()));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::function_call& node) {
                logic::function_call::callable callee = std::visit(overloaded{
                    [this](const std::shared_ptr<logic::function_definition>& callee) -> logic::function_call::callable {
                        return std::shared_ptr<logic::function_definition>(callee);
                    },
                    [this](const std::unique_ptr<logic::expression>& callee) -> logic::function_call::callable {
                        return (*this)(*callee);
                    }
                }, node.callee());

                std::vector<std::unique_ptr<logic::expression>> arguments;
                arguments.reserve(node.arguments().size());
                for (const auto& argument : node.arguments()) {
                    arguments.emplace_back((*this)(*argument));
                }
                auto to_transform = std::make_unique<logic::function_call>(std::move(callee), std::move(arguments));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::conditional_expression& node) {
                std::unique_ptr<logic::expression> condition = (*this)(*node.condition());
                std::unique_ptr<logic::expression> then_expression = (*this)(*node.then_expression());
                std::unique_ptr<logic::expression> else_expression = (*this)(*node.else_expression());
                auto to_transform = std::make_unique<logic::conditional_expression>(
                    std::move(condition),
                    std::move(then_expression),
                    std::move(else_expression),
                    typing::qual_type(node.get_type())
                );
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::set_address& node) {
                std::unique_ptr<logic::expression> destination = (*this)(*node.destination());
                std::unique_ptr<logic::expression> value = (*this)(*node.value());
                auto to_transform = std::make_unique<logic::set_address>(std::move(destination), std::move(value));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::set_variable& node) {
                std::unique_ptr<logic::expression> value = (*this)(*node.value());
                auto to_transform = std::make_unique<logic::set_variable>(m_pass.replace_variable(node.variable()), std::move(value));
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::compound_expression& node) {
                auto to_transform = std::make_unique<logic::compound_expression>(
                    m_pass.transform_control_block(*node.control_block(), {}),
                    typing::qual_type(node.get_type())
                );
                return m_pass.m_expression_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::expression> default_pass::expression_traverser::dispatch(const logic::enumerator_literal& node) {
                return m_pass.m_expression_pass->dispatch(node);
            }

            // statement_traverser methods
            std::shared_ptr<logic::control_block> default_pass::transform_control_block(const logic::control_block& node, std::vector<std::unique_ptr<logic::statement>>&& preamble_statements) {
                if (!m_ran_preamble_pass && m_preamble_visitor) {
                    node.accept(*m_preamble_visitor);
                    m_ran_preamble_pass = true;
                }

                std::shared_ptr<logic::control_block> result = std::make_shared<logic::control_block>();
                std::vector<std::unique_ptr<logic::statement>> statements;
                statements.insert(statements.end(), std::make_move_iterator(preamble_statements.begin()), std::make_move_iterator(preamble_statements.end()));
                statements.reserve(node.statements().size());
                for (const auto& statement : node.statements()) {
                    auto transformed_statement = m_statement_traverser(*statement);
                    if (transformed_statement == nullptr) {
                        continue;
                    }
                    statements.emplace_back(std::move(transformed_statement));
                }
                result->implement(std::move(statements));
                std::unordered_set<std::shared_ptr<logic::variable>> variables_to_replace;
                for (auto symbol : node.symbols()) {
                    result->add(std::shared_ptr<logic::symbol>(symbol));

                    std::shared_ptr<logic::variable> to_replace = std::dynamic_pointer_cast<logic::variable>(symbol);
                    if (to_replace) {
                        variables_to_replace.insert(to_replace);
                    }
                }

                m_replace_variable_contexts.emplace_back(replace_variable_context{
                    .variables_to_replace = std::move(variables_to_replace),
                    .new_context = std::weak_ptr<logic::symbol_context>(result)
                });

                return result;
            }

            std::unique_ptr<logic::statement> default_pass::statement_traverser::dispatch(const logic::expression_statement& node) {
                std::unique_ptr<logic::expression> expression = m_pass.m_expression_traverser(*node.expression());
                auto to_transform = std::make_unique<logic::expression_statement>(std::move(expression));
                return m_pass.m_statement_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::statement> default_pass::statement_traverser::dispatch(const logic::variable_declaration& node) {
                std::shared_ptr<logic::variable> variable = m_pass.replace_variable(node.variable());

                std::unique_ptr<logic::expression> initializer = nullptr;
                if (node.initializer()) {
                    initializer = m_pass.m_expression_traverser(*node.initializer());
                }

                auto to_transform = std::make_unique<logic::variable_declaration>(std::move(variable), std::move(initializer));
                return m_pass.m_statement_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::statement> default_pass::statement_traverser::dispatch(const logic::return_statement& node) {
                std::unique_ptr<logic::expression> value = nullptr;
                if (node.value()) {
                    value = m_pass.m_expression_traverser(*node.value());
                }

                auto to_transform = std::make_unique<logic::return_statement>(
                    std::move(value),
                    std::weak_ptr<logic::function_definition>(node.function()),
                    node.is_compound_return()
                );
                return m_pass.m_statement_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::statement> default_pass::statement_traverser::dispatch(const logic::if_statement& node) {
                std::unique_ptr<logic::expression> condition = m_pass.m_expression_traverser(*node.condition());

                std::shared_ptr<logic::control_block> else_body = nullptr;
                if (node.else_body()) {
                    else_body = m_pass.transform_control_block(*node.else_body(), {});
                }

                auto to_transform = std::make_unique<logic::if_statement>(
                    std::move(condition),
                    m_pass.transform_control_block(*node.then_body(), {}),
                    std::move(else_body)
                );
                return m_pass.m_statement_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::statement> default_pass::statement_traverser::dispatch(const logic::loop_statement& node) {
                std::unique_ptr<logic::expression> condition = m_pass.m_expression_traverser(*node.condition());
                auto to_transform = std::make_unique<logic::loop_statement>(
                    m_pass.transform_control_block(*node.body(), {}),
                    std::move(condition),
                    node.check_condition_first()
                );
                return m_pass.m_statement_pass->dispatch(std::move(to_transform));
            }

            std::unique_ptr<logic::statement> default_pass::statement_traverser::dispatch(const logic::break_statement& node) {
                return m_pass.m_statement_pass->dispatch(node);
            }

            std::unique_ptr<logic::statement> default_pass::statement_traverser::dispatch(const logic::continue_statement& node) {
                return m_pass.m_statement_pass->dispatch(node);
            }

            std::unique_ptr<logic::statement> default_pass::statement_traverser::dispatch(const logic::statement_block& node) {
                auto to_transform = std::make_unique<logic::statement_block>(
                    m_pass.transform_control_block(*node.control_block(), {})
                );
                return m_pass.m_statement_pass->dispatch(std::move(to_transform));
            }

            // default_pass constructor
            default_pass::default_pass(std::unique_ptr<expression_pass>&& expression_transformer,
                std::unique_ptr<statement_pass>&& statement_transformer,
                std::unique_ptr<logic::const_visitor>&& preamble_visitor,
                std::function<std::string(const std::string&)> variable_name_transformer)
                : m_expression_pass(std::move(expression_transformer)),
                m_statement_pass(std::move(statement_transformer)),
                m_expression_traverser(*this),
                m_statement_traverser(*this),
                m_preamble_visitor(std::move(preamble_visitor)),
                m_variable_name_transformer(variable_name_transformer) { }

            std::shared_ptr<logic::variable> default_pass::replace_variable(const std::shared_ptr<logic::variable>& variable) const {
                for (const auto& context : m_replace_variable_contexts) {
                    if (context.variables_to_replace.contains(variable)) {
                        std::string new_name = m_variable_name_transformer(variable->name());
                        return std::make_shared<logic::variable>(
                            std::move(new_name),
                            variable->qualifiers(),
                            typing::qual_type(variable->get_type()),
                            variable->is_global()
                        );
                    }
                }
                return variable;
            }

            bool transform(logic::translation_unit& unit, std::vector<std::unique_ptr<pass>>& passes, int max_passes) {
                for (int i = 0; i < max_passes; i++) {
                    for (auto& pass : passes) {
                        pass->reset();
                        pass->transform(unit);
                    }

                    bool any_pass_mutated = false;
                    for (auto& pass : passes) {
                        if (pass->is_ir_mutated()) {
                            any_pass_mutated = true;
                            break;
                        }
                    }
                    if (!any_pass_mutated) {
                        return true;
                    }
                }
                return false;
            }
        }
    }

    namespace logic {
        void translation_unit::transform(expression_transformer& expression_transformer, statement_transformer& statement_transformer) {
            std::vector<variable_declaration> new_static_variable_declarations;
            new_static_variable_declarations.reserve(m_static_variable_declarations.size());
            for (variable_declaration& declaration : m_static_variable_declarations) {
                auto transformed = statement_transformer(declaration);
                if (!transformed) {
                    continue;
                }

                std::unique_ptr<variable_declaration> new_declaration = dynamic_unique_cast<variable_declaration>(std::move(transformed));
                if (!new_declaration) {
                    throw std::runtime_error("Failed to transform static variable declaration");
                }

                new_static_variable_declarations.emplace_back(std::move(*new_declaration));
            }
            m_static_variable_declarations = std::move(new_static_variable_declarations);

            for (auto& symbol : m_global_context->symbols()) {
                std::shared_ptr<function_definition> function = std::dynamic_pointer_cast<function_definition>(symbol);
                if (function) {
                    function->transform(expression_transformer, statement_transformer);
                }
            }
        }

        void control_block::transform(expression_transformer& expression_transformer, statement_transformer& statement_transformer) {
            for (auto& statement : m_statements) {
                auto transformed = statement_transformer(*statement);
                statement = std::move(transformed);
            }
            // Remove null statements (e.g., from constant propagation removing variable declarations)
            std::erase_if(m_statements, [](const std::unique_ptr<statement>& stmt) { return stmt == nullptr; });
        }
    }
}
