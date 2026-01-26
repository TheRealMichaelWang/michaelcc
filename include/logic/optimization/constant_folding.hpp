#ifndef MICHAELCC_CONSTANT_FOLDING_HPP
#define MICHAELCC_CONSTANT_FOLDING_HPP

#include "logic/optimization/ir_simplify.hpp"
#include "logic/optimization.hpp"
#include "logic/type_info.hpp"

namespace michaelcc {
    namespace optimization {
        class constant_folding_pass final : public default_pass {
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
                default_pass(
                    std::make_unique<expression_pass>(m_layout_calculator), 
                    std::make_unique<default_statement_pass>(), 
                    [](const std::string& name) { return name; }
                ) { }
        };

        std::unique_ptr<pass> inline make_constant_folding_pass(const platform_info& platform_info) {
            std::vector<std::unique_ptr<pass>> passes;
            passes.reserve(2);
            passes.emplace_back(std::make_unique<ir_simplify_pass>(platform_info));
            passes.emplace_back(std::make_unique<constant_folding_pass>(platform_info));
            return std::make_unique<compound_pass>(std::move(passes));
        }
    }
}

#endif