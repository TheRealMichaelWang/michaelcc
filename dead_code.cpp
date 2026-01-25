#include "logic/optimization/dead_code.hpp"
#include "logic/ir.hpp"
#include "logic/analysis/side_effects.hpp"

namespace michaelcc {
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