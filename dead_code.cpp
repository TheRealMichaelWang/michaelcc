#include "logic/dataflow/dead_code.hpp"
#include "logic/logical.hpp"
#include "logic/analysis/side_effects.hpp"

namespace michaelcc {
    namespace dataflow {
        std::unique_ptr<logical_ir::expression> dead_code_pass::expression_pass::dispatch(std::unique_ptr<logical_ir::conditional_expression>&& node) {
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

        std::unique_ptr<logical_ir::statement> dead_code_pass::statement_pass::dispatch(std::unique_ptr<logical_ir::if_statement>&& node) {
            if (is_truey(*node->condition())) {
                mark_ir_mutated();
                return std::make_unique<logical_ir::statement_block>(node->release_then_body());
            }
            if (is_falsey(*node->condition())) {
                mark_ir_mutated();
                if (!node->else_body()) {
                    return nullptr;
                }
                return std::make_unique<logical_ir::statement_block>(node->release_else_body());
            }
            return node;
        }

        std::unique_ptr<logical_ir::statement> dead_code_pass::statement_pass::dispatch(std::unique_ptr<logical_ir::loop_statement>&& node) {
            if (is_falsey(*node->condition())) {
                mark_ir_mutated();
                return nullptr;
            }
            
            return node;
        }

        std::unique_ptr<logical_ir::statement> dead_code_pass::statement_pass::dispatch(std::unique_ptr<logical_ir::expression_statement>&& node) {
            logical_ir::analysis::side_effects_analyzer side_effects_analyzer;
            if (node->expression() == nullptr || !side_effects_analyzer.expression_has_side_effects(*node->expression())) {
                mark_ir_mutated();
                return nullptr;
            }
            return node;
        }
    }
}