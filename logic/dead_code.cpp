#include "logic/optimization/dead_code.hpp"
#include "logic/ir.hpp"
#include "logic/analysis/side_effects.hpp"
#include "logic/typing.hpp"
#include "syntax/tokens.hpp"
#include <memory>

namespace michaelcc {
    namespace logic {
        namespace optimization {
            std::unique_ptr<logic::expression> dead_code_pass::expression_pass::dispatch(std::unique_ptr<logic::conditional_expression>&& node) {
                if (is_truey(*node->condition())) {
                    mark_ir_mutated();
                    return node->release_then_expression();
                }
                if (is_falsey(*node->condition())) {
                    mark_ir_mutated();
                    return node->release_else_expression();
                }
                return node;
            }

            std::unique_ptr<logic::expression> dead_code_pass::expression_pass::dispatch(std::unique_ptr<logic::arithmetic_operator>&& node) {
                logic::analysis::side_effects_analyzer side_effects_analyzer;
                if (node->get_operator() == MICHAELCC_TOKEN_ASTERISK) {
                    if (is_one(*node->left()) && !side_effects_analyzer.expression_has_side_effects(*node->right())) {
                        mark_ir_mutated();
                        return node->release_right();
                    }
                    if (is_one(*node->right()) && !side_effects_analyzer.expression_has_side_effects(*node->left())) {
                        mark_ir_mutated();
                        return node->release_left();
                    }
                    if (is_falsey(*node->left()) && !side_effects_analyzer.expression_has_side_effects(*node->right())) {
                        mark_ir_mutated();
                        return std::make_unique<logic::type_cast>(
                            std::make_unique<logic::integer_constant>(0, typing::qual_type::owning(std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS))),
                            typing::qual_type(node->get_type())
                        );
                    }
                    if (is_falsey(*node->right()) && !side_effects_analyzer.expression_has_side_effects(*node->left())) {
                        mark_ir_mutated();
                        return std::make_unique<logic::type_cast>(
                            std::make_unique<logic::integer_constant>(0, typing::qual_type::owning(std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS))),
                            typing::qual_type(node->get_type())
                        );
                    }
                }
                else if (node->get_operator() == MICHAELCC_TOKEN_SLASH) {
                    if (is_falsey(*node->left()) && !side_effects_analyzer.expression_has_side_effects(*node->right())) {
                        mark_ir_mutated();
                        return std::make_unique<logic::type_cast>(
                            std::make_unique<logic::integer_constant>(0, typing::qual_type::owning(std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS))),
                            typing::qual_type(node->get_type())
                        );
                    }
                    if (is_one(*node->right())) {
                        mark_ir_mutated();
                        return node->release_left();
                    }
                }
                else if (node->get_operator() == MICHAELCC_TOKEN_PLUS) {
                    if (is_falsey(*node->left())) {
                        mark_ir_mutated();
                        return node->release_right();
                    }
                    if (is_falsey(*node->right())) {
                        mark_ir_mutated();
                        return node->release_left();
                    }
                }
                else if (node->get_operator() == MICHAELCC_TOKEN_MINUS) {
                    if (is_falsey(*node->left())) {
                        mark_ir_mutated();
                        return std::make_unique<logic::unary_operation>(
                            MICHAELCC_TOKEN_MINUS,
                            node->release_left()
                        );
                    }
                    if (is_falsey(*node->right())) {
                        mark_ir_mutated();
                        return node->release_left();
                    }
                }
                else if (node->get_operator() == MICHAELCC_TOKEN_AND) {
                    if (is_falsey(*node->left()) && !side_effects_analyzer.expression_has_side_effects(*node->right())) {
                        mark_ir_mutated();
                        return std::make_unique<logic::integer_constant>(0, typing::qual_type::owning(std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS)));
                    }
                    if (is_falsey(*node->right()) && !side_effects_analyzer.expression_has_side_effects(*node->left())) {
                        mark_ir_mutated();
                        return std::make_unique<logic::integer_constant>(0, typing::qual_type::owning(std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS)));
                    }
                }
                else if (node->get_operator() == MICHAELCC_TOKEN_OR) {
                    if (is_truey(*node->left()) && !side_effects_analyzer.expression_has_side_effects(*node->right())) {
                        mark_ir_mutated();
                        return node->release_right();
                    }
                    if (is_truey(*node->right()) && !side_effects_analyzer.expression_has_side_effects(*node->left())) {
                        mark_ir_mutated();
                        return node->release_left();
                    }
                }
                return node;
            }

            std::unique_ptr<logic::statement> dead_code_pass::statement_pass::dispatch(std::unique_ptr<logic::if_statement>&& node) {
                if (is_truey(*node->condition())) {
                    mark_ir_mutated();
                    return std::make_unique<logic::statement_block>(node->release_then_body());
                }
                if (is_falsey(*node->condition())) {
                    mark_ir_mutated();
                    if (!node->else_body()) {
                        return nullptr;
                    }
                    return std::make_unique<logic::statement_block>(node->release_else_body());
                }
                return node;
            }

            std::unique_ptr<logic::statement> dead_code_pass::statement_pass::dispatch(std::unique_ptr<logic::loop_statement>&& node) {
                if (is_falsey(*node->condition())) {
                    if (node->check_condition_first()) {
                        // while/for loop - safe to remove
                        mark_ir_mutated();
                        return nullptr;
                    } else {
                        // do-while loop - body executes once
                        mark_ir_mutated();
                        return std::make_unique<logic::statement_block>(node->release_body());
                    }
                }
                return node;
            }

            std::unique_ptr<logic::statement> dead_code_pass::statement_pass::dispatch(std::unique_ptr<logic::expression_statement>&& node) {
                logic::analysis::side_effects_analyzer side_effects_analyzer;
                if (node->expression() == nullptr || !side_effects_analyzer.expression_has_side_effects(*node->expression())) {
                    mark_ir_mutated();
                    return nullptr;
                }
                return node;
            }
        }
    }
}
