#ifndef MICHAELCC_DEAD_CODE_HPP
#define MICHAELCC_DEAD_CODE_HPP

#include "logic/dataflow.hpp"
#include "logic/logical.hpp"

namespace michaelcc {
    namespace dataflow {
        namespace dead_code {
            class expression_pass : public default_expression_pass {
            public:
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::conditional_expression>&& node) override;
            };

            class statement_pass : public default_statement_pass {
            public:
                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::if_statement>&& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::loop_statement>&& node) override;
                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::expression_statement>&& node) override;
            };

            inline bool is_truey(const logical_ir::expression& expression) {
                if (auto integer_constant = dynamic_cast<const logical_ir::integer_constant*>(&expression)) {
                    return integer_constant->value() != 0;
                }
                return false;
            }

            inline bool is_falsey(const logical_ir::expression& expression) {
                if (auto integer_constant = dynamic_cast<const logical_ir::integer_constant*>(&expression)) {
                    return integer_constant->value() == 0;
                }
                return false;
            }
        }

        class dead_code_pass final : public transform_pass {
        public:
            dead_code_pass() : transform_pass(
                std::make_unique<dead_code::expression_pass>(), 
                std::make_unique<dead_code::statement_pass>(), 
                [](const std::string& name) { return name; }
            ) { }
        };
    }
}

#endif