#ifndef MICHAELCC_CONSTANT_FOLDING_HPP
#define MICHAELCC_CONSTANT_FOLDING_HPP

#include "logic/optimization.hpp"
#include "logic/type_info.hpp"

namespace michaelcc {
    namespace optimization {
        class constant_folding_pass final : public transform_pass {
        private:
            class expression_pass : public default_expression_pass {
            private:
                type_layout_calculator& m_layout_calculator;

            public:
                expression_pass(type_layout_calculator& layout_calculator) 
                    : m_layout_calculator(layout_calculator) {}

                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::arithmetic_operator>&& node) override;
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::unary_operation>&& node) override;

                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::type_cast>&& node) override;
            };

            type_layout_calculator m_layout_calculator;
        
        public:
            constant_folding_pass(const platform_info& platform_info) 
                : m_layout_calculator(platform_info),
                transform_pass(
                    std::make_unique<expression_pass>(m_layout_calculator), 
                    std::make_unique<default_statement_pass>(), 
                    [](const std::string& name) { return name; }
                ) { }
        };
    }
}

#endif