#include "logic/optimization.hpp"
#include "logic/logical.hpp"
#include "logic/typing.hpp"
#include "logic/logical.hpp"
#include <memory>

namespace michaelcc {
    namespace optimization {
        // expression_traverser constructor
        transform_pass::expression_traverser::expression_traverser(transform_pass& pass) : m_pass(pass) { }

        // expression_traverser dispatch methods
        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::integer_constant& node) {
            return m_pass.m_expression_pass->dispatch(node);
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::floating_constant& node) {
            return m_pass.m_expression_pass->dispatch(node);
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::string_constant& node) {
            return m_pass.m_expression_pass->dispatch(node);
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::variable_reference& node) {
            std::shared_ptr<logical_ir::variable> variable = m_pass.replace_variable(node.get_variable());
            auto to_transform = std::make_unique<logical_ir::variable_reference>(std::move(variable));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::function_reference& node) {
            return m_pass.m_expression_pass->dispatch(node);
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::increment_operator& node) {
            auto destination = std::visit(overloaded{
                [this](const std::unique_ptr<logical_ir::expression>& destination) -> logical_ir::increment_operator::destination_type {
                    return (*this)(*destination);
                },
                [this](const std::shared_ptr<logical_ir::variable>& destination) -> logical_ir::increment_operator::destination_type {
                    return m_pass.replace_variable(destination);
                }
            }, node.destination());

            std::optional<std::unique_ptr<logical_ir::expression>> increment_amount = std::nullopt;
            if (node.increment_amount().has_value()) {
                increment_amount = std::make_optional((*this)(*(node.increment_amount().value())));
            }

            auto to_transform = std::make_unique<logical_ir::increment_operator>(std::move(destination), std::move(increment_amount));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::arithmetic_operator& node) {
            std::unique_ptr<logical_ir::expression> left = (*this)(*node.left());
            std::unique_ptr<logical_ir::expression> right = (*this)(*node.right());

            auto to_transform = std::make_unique<logical_ir::arithmetic_operator>(node.get_operator(), std::move(left), std::move(right), typing::qual_type(node.get_type()));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::unary_operation& node) {
            std::unique_ptr<logical_ir::expression> operand = (*this)(*node.operand());
            auto to_transform = std::make_unique<logical_ir::unary_operation>(node.get_operator(), std::move(operand));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::type_cast& node) {
            std::unique_ptr<logical_ir::expression> operand = (*this)(*node.operand());
            auto to_transform = std::make_unique<logical_ir::type_cast>(std::move(operand), typing::qual_type(node.get_type()));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::address_of& node) {
            std::unique_ptr<logical_ir::address_of> to_transform = std::visit(overloaded{
                [this](const std::unique_ptr<logical_ir::array_index>& operand) -> std::unique_ptr<logical_ir::address_of> {
                    auto transformed_operand = dynamic_unique_cast<logical_ir::array_index>((*this)(*operand));
                    if (transformed_operand == nullptr) {
                        throw std::runtime_error("Failed to transform array index");
                    }
                    return std::make_unique<logical_ir::address_of>(std::move(transformed_operand));
                },
                [this](const std::unique_ptr<logical_ir::member_access>& operand) -> std::unique_ptr<logical_ir::address_of> {
                    auto transformed_operand = dynamic_unique_cast<logical_ir::member_access>((*this)(*operand));
                    if (transformed_operand == nullptr) {
                        throw std::runtime_error("Failed to transform member access");
                    }
                    return std::make_unique<logical_ir::address_of>(std::move(transformed_operand));
                },
                [this](const std::shared_ptr<logical_ir::variable>& operand) -> std::unique_ptr<logical_ir::address_of> {
                    return std::make_unique<logical_ir::address_of>(m_pass.replace_variable(operand));
                }
            }, node.operand());
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::dereference& node) {
            std::unique_ptr<logical_ir::expression> operand = (*this)(*node.operand());
            auto to_transform = std::make_unique<logical_ir::dereference>(std::move(operand));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::member_access& node) {
            std::unique_ptr<logical_ir::expression> base = (*this)(*node.base());
            auto to_transform = std::make_unique<logical_ir::member_access>(std::move(base), node.member(), node.is_dereference());
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::array_index& node) {
            std::unique_ptr<logical_ir::expression> base = (*this)(*node.base());
            std::unique_ptr<logical_ir::expression> index = (*this)(*node.index());
            auto to_transform = std::make_unique<logical_ir::array_index>(std::move(base), std::move(index));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::array_initializer& node) {
            std::vector<std::unique_ptr<logical_ir::expression>> initializers;
            initializers.reserve(node.initializers().size());
            for (const auto& initializer : node.initializers()) {
                initializers.emplace_back((*this)(*initializer));
            }
            auto to_transform = std::make_unique<logical_ir::array_initializer>(std::move(initializers), typing::qual_type(node.element_type()));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::allocate_array& node) {
            std::vector<std::unique_ptr<logical_ir::expression>> dimensions;
            dimensions.reserve(node.dimensions().size());
            for (const auto& dimension : node.dimensions()) {
                dimensions.emplace_back((*this)(*dimension));
            }
            auto to_transform = std::make_unique<logical_ir::allocate_array>(std::move(dimensions), typing::qual_type(node.get_type()));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::struct_initializer& node) {
            std::vector<logical_ir::struct_initializer::member_initializer> initializers;
            initializers.reserve(node.initializers().size());
            for (const auto& initializer : node.initializers()) {
                initializers.emplace_back(initializer.member_name, (*this)(*initializer.initializer));
            }
            auto to_transform = std::make_unique<logical_ir::struct_initializer>(std::move(initializers), std::shared_ptr<typing::struct_type>(node.struct_type()));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::union_initializer& node) {
            std::unique_ptr<logical_ir::expression> initializer = (*this)(*node.initializer());
            auto to_transform = std::make_unique<logical_ir::union_initializer>(std::move(initializer), std::shared_ptr<typing::union_type>(node.union_type()), typing::member(node.target_member()));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::function_call& node) {
            logical_ir::function_call::callable callee = std::visit(overloaded{
                [this](const std::shared_ptr<logical_ir::function_definition>& callee) -> logical_ir::function_call::callable {
                    return std::shared_ptr<logical_ir::function_definition>(callee);
                },
                [this](const std::shared_ptr<logical_ir::expression>& callee) -> logical_ir::function_call::callable {
                    return (*this)(*callee);
                }
            }, node.callee());

            std::vector<std::unique_ptr<logical_ir::expression>> arguments;
            arguments.reserve(node.arguments().size());
            for (const auto& argument : node.arguments()) {
                arguments.emplace_back((*this)(*argument));
            }
            auto to_transform = std::make_unique<logical_ir::function_call>(std::move(callee), std::move(arguments));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::conditional_expression& node) {
            std::unique_ptr<logical_ir::expression> condition = (*this)(*node.condition());
            std::unique_ptr<logical_ir::expression> then_expression = (*this)(*node.then_expression());
            std::unique_ptr<logical_ir::expression> else_expression = (*this)(*node.else_expression());
            auto to_transform = std::make_unique<logical_ir::conditional_expression>(
                std::move(condition),
                std::move(then_expression),
                std::move(else_expression),
                typing::qual_type(node.get_type())
            );
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::set_address& node) {
            std::unique_ptr<logical_ir::expression> destination = (*this)(*node.destination());
            std::unique_ptr<logical_ir::expression> value = (*this)(*node.value());
            auto to_transform = std::make_unique<logical_ir::set_address>(std::move(destination), std::move(value));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::set_variable& node) {
            std::unique_ptr<logical_ir::expression> value = (*this)(*node.value());
            auto to_transform = std::make_unique<logical_ir::set_variable>(m_pass.replace_variable(node.variable()), std::move(value));
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::compound_expression& node) {
            auto to_transform = std::make_unique<logical_ir::compound_expression>(
                m_pass.transform_control_block(*node.control_block()), 
                m_pass.m_expression_traverser(*node.return_expression())
            );
            return m_pass.m_expression_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::expression> transform_pass::expression_traverser::dispatch(const logical_ir::enumerator_literal& node) {
            return m_pass.m_expression_pass->dispatch(node);
        }

        // statement_traverser methods
        std::shared_ptr<logical_ir::control_block> transform_pass::transform_control_block(const logical_ir::control_block& node) {
            std::shared_ptr<logical_ir::control_block> result = std::make_shared<logical_ir::control_block>();
            std::vector<std::unique_ptr<logical_ir::statement>> statements;
            statements.reserve(node.statements().size());
            for (const auto& statement : node.statements()) {
                auto transformed_statement = m_statement_traverser(*statement);
                if(transformed_statement == nullptr) {
                    continue;
                }
                statements.emplace_back(std::move(transformed_statement));
            }
            result->implement(std::move(statements));
            std::unordered_set<std::shared_ptr<logical_ir::variable>> variables_to_replace;
            for (auto symbol : node.symbols()) {
                result->add(std::shared_ptr<logical_ir::symbol>(symbol));
                
                std::shared_ptr<logical_ir::variable> to_replace = std::dynamic_pointer_cast<logical_ir::variable>(symbol);
                if (to_replace) {
                    variables_to_replace.emplace(to_replace);
                }
            }

            m_replace_variable_contexts.emplace_back(replace_variable_context{
                .variables_to_replace = std::move(variables_to_replace),
                .new_context = std::weak_ptr<logical_ir::symbol_context>(result)
            });

            return result;
        }

        std::unique_ptr<logical_ir::statement> transform_pass::statement_traverser::dispatch(const logical_ir::expression_statement& node) {
            std::unique_ptr<logical_ir::expression> expression = m_pass.m_expression_traverser(*node.expression());
            auto to_transform = std::make_unique<logical_ir::expression_statement>(std::move(expression));
            return m_pass.m_statement_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::statement> transform_pass::statement_traverser::dispatch(const logical_ir::variable_declaration& node) {
            std::shared_ptr<logical_ir::variable> variable = m_pass.replace_variable(node.variable());
            
            std::unique_ptr<logical_ir::expression> initializer = nullptr;
            if (node.initializer()) {
                initializer = m_pass.m_expression_traverser(*node.initializer());
            }
            
            auto to_transform = std::make_unique<logical_ir::variable_declaration>(std::move(variable), std::move(initializer));
            return m_pass.m_statement_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::statement> transform_pass::statement_traverser::dispatch(const logical_ir::return_statement& node) {
            std::unique_ptr<logical_ir::expression> value = nullptr;
            if (node.value()) {
                value = m_pass.m_expression_traverser(*node.value());
            }
            
            auto to_transform = std::make_unique<logical_ir::return_statement>(std::move(value), std::weak_ptr<logical_ir::function_definition>());
            return m_pass.m_statement_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::statement> transform_pass::statement_traverser::dispatch(const logical_ir::if_statement& node) {
            std::unique_ptr<logical_ir::expression> condition = m_pass.m_expression_traverser(*node.condition());
            
            std::shared_ptr<logical_ir::control_block> else_body = nullptr;
            if (node.else_body()) {
                else_body = m_pass.transform_control_block(*node.else_body());
            }
            
            auto to_transform = std::make_unique<logical_ir::if_statement>(
                std::move(condition), 
                m_pass.transform_control_block(*node.then_body()),
                std::move(else_body)
            );
            return m_pass.m_statement_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::statement> transform_pass::statement_traverser::dispatch(const logical_ir::loop_statement& node) {
            std::unique_ptr<logical_ir::expression> condition = m_pass.m_expression_traverser(*node.condition());
            auto to_transform = std::make_unique<logical_ir::loop_statement>(
                m_pass.transform_control_block(*node.body()),
                std::move(condition),
                node.check_condition_first()
            );
            return m_pass.m_statement_pass->dispatch(std::move(to_transform));
        }

        std::unique_ptr<logical_ir::statement> transform_pass::statement_traverser::dispatch(const logical_ir::break_statement& node) {
            return m_pass.m_statement_pass->dispatch(node);
        }

        std::unique_ptr<logical_ir::statement> transform_pass::statement_traverser::dispatch(const logical_ir::continue_statement& node) {
            return m_pass.m_statement_pass->dispatch(node);
        }
        
        std::unique_ptr<logical_ir::statement> transform_pass::statement_traverser::dispatch(const logical_ir::statement_block& node) {
            auto to_transform = std::make_unique<logical_ir::statement_block>(m_pass.transform_control_block(*node.control_block()));
            return m_pass.m_statement_pass->dispatch(std::move(to_transform));
        }

        // transform_pass constructor
        transform_pass::transform_pass(std::unique_ptr<expression_pass>&& expression_transformer, 
            std::unique_ptr<statement_pass>&& statement_transformer, 
            std::function<std::string(const std::string&)> variable_name_transformer,
            std::vector<replace_variable_context> replace_variable_contexts) 
            : m_expression_pass(std::move(expression_transformer)), 
            m_statement_pass(std::move(statement_transformer)),
            m_expression_traverser(*this),
            m_statement_traverser(*this),
            m_variable_name_transformer(variable_name_transformer),
            m_replace_variable_contexts() { }

        std::shared_ptr<logical_ir::variable> transform_pass::replace_variable(const std::shared_ptr<logical_ir::variable>& variable) const {
            for (const auto& context : m_replace_variable_contexts) {
                if (context.variables_to_replace.contains(variable)) {
                    std::string new_name = m_variable_name_transformer(variable->name());
                    return std::make_shared<logical_ir::variable>(
                        std::move(new_name), 
                        variable->qualifiers(), 
                        typing::qual_type(variable->get_type()), 
                        variable->is_global(), 
                        std::weak_ptr<logical_ir::symbol_context>(context.new_context)
                    );
                }
            }
            return variable;
        }

        int transform_pass::transform(logical_ir::translation_unit&unit, std::vector<std::unique_ptr<transform_pass>>& passes, int max_passes) {
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
                    return i;
                }
            }
            return max_passes;
        }
    }

    namespace logical_ir {
        void translation_unit::transform(expression_transformer& expression_transformer, statement_transformer& statement_transformer) {
            std::vector<variable_declaration> new_static_variable_declarations;
            new_static_variable_declarations.reserve(m_static_variable_declarations.size());
            for(variable_declaration& declaration : m_static_variable_declarations) {
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
