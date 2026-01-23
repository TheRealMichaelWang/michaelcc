#ifndef MICHAELCC_IR_SIMPLIFY_HPP
#define MICHAELCC_IR_SIMPLIFY_HPP

#include "logic/dataflow.hpp"
#include "logic/logical.hpp"

namespace michaelcc {
    namespace dataflow {
        class ir_simplify_pass final : public transform_pass {
        private:
            class expression_pass : public default_expression_pass {
            public:
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::set_address>&& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::dereference>&& node) override;
            };
        public:
            ir_simplify_pass() : transform_pass(
                std::make_unique<expression_pass>(), 
                std::make_unique<default_statement_pass>(), 
                [](const std::string& name) { return name; }
            ) { }
        };
    }
}
#endif