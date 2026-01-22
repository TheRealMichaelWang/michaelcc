#include "logic/dataflow/dead_code.hpp"
#include "logic/logical.hpp"

namespace michaelcc {
    namespace dataflow {
        namespace dead_code {
            std::unique_ptr<logical_ir::expression> expression_pass::dispatch(std::unique_ptr<logical_ir::conditional_expression>&& node) {
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

            std::unique_ptr<logical_ir::statement> statement_pass::dispatch(std::unique_ptr<logical_ir::if_statement>&& node) {
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

            std::unique_ptr<logical_ir::statement> statement_pass::dispatch(std::unique_ptr<logical_ir::loop_statement>&& node) {
                if (is_falsey(*node->condition())) {
                    mark_ir_mutated();
                    return nullptr;
                }
                
                return node;
            }
        }
    }
}