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
            };
        }

        extern transform_pass constant_folding_pass;
    }
}

#endif