#ifndef MICHAELCC_CONSTANT_FOLDING_HPP
#define MICHAELCC_CONSTANT_FOLDING_HPP

#include "logic/dataflow.hpp"

namespace michaelcc {
    namespace dataflow {
        namespace constant_folding {
            class expression_pass : public default_expression_pass {
            public:
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::arithmetic_operator>&& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::unary_operation>&& node) override;

                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::type_cast>&& node) override;
            };
        }

        inline transform_pass constant_folding_pass() {
            return transform_pass(
                std::make_unique<constant_folding::expression_pass>(), 
                std::make_unique<default_statement_pass>(), 
                [](const std::string& name) { return name; }
            );
        }
    }
}

#endif