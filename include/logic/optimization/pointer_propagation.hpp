#ifndef MICHAELCC_POINTER_PROPAGATION_HPP
#define MICHAELCC_POINTER_PROPAGATION_HPP

#include "logic/optimization.hpp"
#include "logic/ir.hpp"
#include <memory>
#include <unordered_map>

namespace michaelcc {
    namespace optimization {
        class pointer_propagation_pass final : public default_pass {
        private:
            std::unordered_map<std::shared_ptr<logic::variable>, std::shared_ptr<logic::variable>> m_variable_map;

            class expression_pass : public default_expression_pass {
            private:
                pointer_propagation_pass& m_pass;

            public:
                expression_pass(pointer_propagation_pass& pass) : m_pass(pass) {}

                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::variable_reference>&& node) override {
                    auto variable = node->get_variable();
                    if (m_pass.m_variable_map.contains(variable)) {
                        mark_ir_mutated();
                        return std::make_unique<logic::address_of>(std::shared_ptr<logic::variable>(m_pass.m_variable_map[variable]));
                    }
                    return node;
                }

                std::unique_ptr<logic::expression> dispatch(std::unique_ptr<logic::set_variable>&& node) override {
                    auto variable = node->variable();
                    if (m_pass.m_variable_map.contains(variable)) {
                        mark_ir_mutated();
                        return std::make_unique<logic::set_variable>(std::shared_ptr<logic::variable>(m_pass.m_variable_map[variable]), node->release_value());
                    }
                    return node;
                }
            };

            class pointer_identifier : public logic::const_visitor {
            private:
                pointer_propagation_pass& m_pass;

            public:
                pointer_identifier(pointer_propagation_pass& pass) : m_pass(pass) {}
            
            protected:
                void visit(const logic::variable_declaration& node) override {
                    if (!(node.variable()->get_type().qualifiers() & typing::CONST_TYPE_QUALIFIER)) {
                        return;
                    }

                    if (auto* address_of = dynamic_cast<const logic::address_of*>(node.initializer().get())) {
                        if (std::holds_alternative<std::shared_ptr<logic::variable>>(address_of->operand())) {
                            m_pass.m_variable_map[node.variable()] = std::get<std::shared_ptr<logic::variable>>(address_of->operand());
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
            pointer_propagation_pass() : default_pass(
                std::make_unique<expression_pass>(*this),
                std::make_unique<default_statement_pass>(),
                std::make_unique<pointer_identifier>(*this)
            ) { }
        };
    }
}
#endif