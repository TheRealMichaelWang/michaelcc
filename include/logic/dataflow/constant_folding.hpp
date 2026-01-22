#ifndef MICHAELCC_CONSTANT_FOLDING_HPP
#define MICHAELCC_CONSTANT_FOLDING_HPP

#include "logic/dataflow.hpp"

namespace michaelcc {
    namespace dataflow {
        class constant_folding_pass final : public transform_pass {
        private:
            class expression_pass : public default_expression_pass {
            public:
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::arithmetic_operator>&& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::unary_operation>&& node) override;

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::type_cast>&& node) override;
            };
        
        public:
            constant_folding_pass() : transform_pass(
                std::make_unique<expression_pass>(), 
                std::make_unique<default_statement_pass>(), 
                [](const std::string& name) { return name; }
            ) { }
        };
    }
}

#endif