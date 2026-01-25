#ifndef MICHAELCC_IR_SIMPLIFY_HPP
#define MICHAELCC_IR_SIMPLIFY_HPP

#include "logic/optimization.hpp"
#include "logic/ir.hpp"

namespace michaelcc {
    namespace optimization {
        class ir_simplify_pass final : public transform_pass {
        private:
            class expression_pass : public default_expression_pass {
            protected:
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_address>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::dereference>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::compound_expression>&& node) override;

                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::member_access>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::array_index>&& node) override;
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