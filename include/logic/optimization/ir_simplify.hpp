#ifndef MICHAELCC_IR_SIMPLIFY_HPP
#define MICHAELCC_IR_SIMPLIFY_HPP

#include "logic/optimization.hpp"
#include "logic/ir.hpp"
#include "logic/type_info.hpp"

namespace michaelcc {
    namespace optimization {
        class ir_simplify_pass final : public default_pass {
        private:
            class expression_pass : public default_expression_pass {
            private:
                ir_simplify_pass& m_pass;

            public:
                expression_pass(ir_simplify_pass& pass) : m_pass(pass) {}

            protected:
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_address>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::dereference>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::compound_expression>&& node) override;

                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::member_access>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::array_index>&& node) override;

                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::type_cast>&& node) override;
            };

            class statement_pass : public default_statement_pass {
            protected:
                std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::expression_statement>&& node) override;
            };

            type_layout_calculator m_layout_calculator;
        public:
            ir_simplify_pass(const platform_info& platform_info) : m_layout_calculator(platform_info), default_pass(
                std::make_unique<expression_pass>(*this), 
                std::make_unique<default_statement_pass>(), 
                [](const std::string& name) { return name; }
            ) { }
        };
    }
}
#endif