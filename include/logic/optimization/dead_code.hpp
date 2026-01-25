#ifndef MICHAELCC_DEAD_CODE_HPP
#define MICHAELCC_DEAD_CODE_HPP

#include "logic/optimization.hpp"
#include "logic/ir.hpp"

namespace michaelcc {
    namespace optimization {
        class dead_code_pass final : public default_pass {
        private:
            class expression_pass : public default_expression_pass {
            public:
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::conditional_expression>&& node) override;
            };

            class statement_pass : public default_statement_pass {
            public:
                std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::if_statement>&& node) override;
                std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::loop_statement>&& node) override;
                std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::expression_statement>&& node) override;
            };

            static bool is_truey(const logic::expression& expression) {
                if (auto integer_constant = dynamic_cast<const logic::integer_constant*>(&expression)) {
                    return integer_constant->value() != 0;
                }
                return false;
            }

            static bool is_falsey(const logic::expression& expression) {
                if (auto integer_constant = dynamic_cast<const logic::integer_constant*>(&expression)) {
                    return integer_constant->value() == 0;
                }
                return false;
            }
        public:
            dead_code_pass() : default_pass(
                std::make_unique<expression_pass>(), 
                std::make_unique<statement_pass>(), 
                [](const std::string& name) { return name; }
            ) { }
        };
    }
}

#endif