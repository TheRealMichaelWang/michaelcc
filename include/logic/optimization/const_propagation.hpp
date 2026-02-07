#ifndef MICHAELCC_CONST_PROPAGATION_HPP
#define MICHAELCC_CONST_PROPAGATION_HPP

#include "logic/optimization.hpp"
#include "logic/ir.hpp"
#include "logic/analysis/constant_analysis.hpp"
#include <memory>
#include <unordered_map>

namespace michaelcc {
    namespace optimization {
        class const_propagation_pass final : public default_pass {
        private:
            const platform_info m_platform_info;
            std::unordered_map<std::shared_ptr<logic::variable>, std::unique_ptr<logic::expression>> m_variable_map;        

            class expression_pass : public default_expression_pass {
            private:
                const_propagation_pass& m_pass;

            public:
                expression_pass(const_propagation_pass& pass) : m_pass(pass) {}

            protected:
                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::variable_reference>&& node) override {
                    if (m_pass.m_variable_map.contains(node->get_variable())) {
                        mark_ir_mutated();
                        auto constant_cloner = logic::analysis::constant_cloner(m_pass.m_platform_info);
                        auto constant = constant_cloner(*m_pass.m_variable_map[node->get_variable()]);
                        return constant;
                    }
                    return node;
                }
            };

            class statement_pass : public default_statement_pass {
            private:
                const_propagation_pass& m_pass;

            public:
                statement_pass(const_propagation_pass& pass) : m_pass(pass) {}

            protected:
                std::unique_ptr<logic::statement> dispatch(std::unique_ptr<logic::variable_declaration>&& node) override {
                    if (m_pass.m_variable_map.contains(node->variable())) {
                        mark_ir_mutated();
                        return std::make_unique<logic::expression_statement>(
                            node->release_initializer()
                        );
                    }
                    return node;
                }
            };

            class const_propagation_identifier : public logic::const_visitor {
            private:
                const_propagation_pass& m_pass;

            public:
                const_propagation_identifier(const_propagation_pass& pass) : m_pass(pass) {}

            protected:
                void visit(const logic::variable_declaration& node) override {
                    if ((node.variable()->get_type().qualifiers() & typing::CONST_TYPE_QUALIFIER) && node.initializer()) {
                        logic::analysis::constant_cloner cloner(m_pass.m_platform_info);
                        auto constant = cloner(*node.initializer());
                        if (constant) {
                            m_pass.m_variable_map[node.variable()] = std::move(constant);
                        }
                    }
                }
                
                void visit(const logic::address_of& node) override {
                    if (std::holds_alternative<std::shared_ptr<logic::variable>>(node.operand())) {
                        std::shared_ptr<logic::variable> variable = std::get<std::shared_ptr<logic::variable>>(node.operand());
                        if (m_pass.m_variable_map.contains(variable)) {
                            m_pass.m_variable_map.erase(variable);
                        }
                    }
                }
            };
        
        public:
            const_propagation_pass(const platform_info platform_info) : m_platform_info(platform_info), default_pass(
                std::make_unique<expression_pass>(*this),
                std::make_unique<statement_pass>(*this),
                std::make_unique<const_propagation_identifier>(*this)
            ) { }
        };
    }
}

#endif