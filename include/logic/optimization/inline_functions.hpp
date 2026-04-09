#ifndef MICHAELCC_INLINE_FUNCTIONS_HPP
#define MICHAELCC_INLINE_FUNCTIONS_HPP

#include "logic/optimization.hpp"
#include "logic/ir.hpp"
#include "logic/typing.hpp"
#include <memory>

namespace michaelcc {
    namespace logic {
        namespace optimization {
            class inline_functions_pass final : public default_pass {
            private:
                class function_transform_pass final : public default_pass {
                private:
                    class statement_pass : public default_statement_pass {
                    public:
                        std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::return_statement>&& node) override {
                            if (!node->is_compound_return()) {
                                mark_ir_mutated();
                                return std::make_unique<logic::return_statement>(
                                    std::move(node->release_value()),
                                    std::weak_ptr<logic::function_definition>(node->function()),
                                    true
                                );
                            }
                            return node;
                        }
                    };

                public:
                    function_transform_pass()
                        : default_pass(
                            std::make_unique<default_expression_pass>(),
                            std::make_unique<statement_pass>()
                        ) { }

                    static std::unique_ptr<logic::compound_expression> transform_function(const logic::function_definition& function, std::vector<std::unique_ptr<logic::expression>>&& arguments) {
                        if (arguments.size() != function.parameters().size()) {
                            throw std::runtime_error("Invalid number of arguments for function " + function.name());
                        }

                        std::vector<std::unique_ptr<logic::statement>> statements;
                        statements.reserve(function.parameters().size() + function.statements().size());

                        for (size_t i = 0; i < function.parameters().size(); i++) {
                            const auto& parameter = function.parameters()[i];
                            statements.emplace_back(std::make_unique<logic::variable_declaration>(
                                std::shared_ptr<logic::variable>(parameter),
                                std::move(arguments[i])
                            ));
                        }

                        auto transform_pass = std::make_unique<function_transform_pass>();
                        auto control_block = transform_pass->transform_control_block(function, std::move(statements));

                        return std::make_unique<logic::compound_expression>(
                            std::move(control_block),
                            typing::qual_type(function.return_type())
                        );
                    }
                };

                class expression_pass : public default_expression_pass {
                public:
                    std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::function_call>&& node) override {
                        if (std::holds_alternative<std::shared_ptr<logic::function_definition>>(node->callee())) {
                            std::shared_ptr<logic::function_definition> function = std::get<std::shared_ptr<logic::function_definition>>(node->callee());
                            if (function->should_inline()) {
                                mark_ir_mutated();
                                auto arguments = node->release_arguments();
                                return function_transform_pass::transform_function(*function, std::move(arguments));
                            }
                        }

                        return node;
                    }
                };

            public:
                inline_functions_pass()
                    : default_pass(
                        std::make_unique<expression_pass>(),
                        std::make_unique<default_statement_pass>()
                    ) { }
            };
        }
    }
}

#endif
