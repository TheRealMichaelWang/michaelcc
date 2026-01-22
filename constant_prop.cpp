#include "logic/dataflow/constant_prop.hpp"
#include "logic/logical.hpp"
#include <memory>
#include <variant>

namespace michaelcc {
    namespace dataflow {
        void variable_use_analyzer::visit(const logical_ir::variable_declaration& node) {
            m_variable_metrics.insert({ 
                node.variable(), 
                { .is_mutated = false, .num_uses = 0 } 
            });
        }

        void variable_use_analyzer::visit(const logical_ir::set_variable& node) {
            auto& metrics = m_variable_metrics[node.variable()];
            metrics.is_mutated = true;

            if (m_dead_expressions.contains(&node)) {
                return;
            }
            metrics.num_uses++;
        }

        void variable_use_analyzer::visit(const logical_ir::increment_operator& node) {
            if (std::holds_alternative<std::shared_ptr<logical_ir::variable>>(node.destination())) {
                auto& metrics = m_variable_metrics[std::get<std::shared_ptr<logical_ir::variable>>(node.destination())];
                metrics.is_mutated = true;


                if (m_dead_expressions.contains(&node)) {
                    return;
                }
                metrics.num_uses++;
            }
        }

        void variable_use_analyzer::visit(const logical_ir::variable_reference& node) {
            auto& metrics = m_variable_metrics[node.get_variable()];
            metrics.num_uses++;
        }

        void variable_use_analyzer::visit(const logical_ir::address_of& node) {
            if (std::holds_alternative<std::shared_ptr<logical_ir::variable>>(node.operand())) {
                auto& metrics = m_variable_metrics[std::get<std::shared_ptr<logical_ir::variable>>(node.operand())];
                metrics.num_uses++;
            }
        }

        void variable_use_analyzer::visit(const logical_ir::function_definition& node) {
            for (const auto& parameter : node.parameters()) {
                // marking parameter as mutated and more than one use ensures it is not optimized away in any form
                m_variable_metrics.insert({ 
                    parameter, 
                    { .is_mutated = true, .num_uses = 1 } 
                });
            }
        }

        void variable_use_analyzer::visit(const logical_ir::expression_statement& node) {
            m_dead_expressions.insert(node.expression().get());
        }


        std::unique_ptr<logical_ir::expression> constant_prop_pass::expression_pass::dispatch(std::unique_ptr<logical_ir::set_variable>&& node) {
            auto& variable = node->variable();

            if (m_pass.can_remove_variable(variable)) {
                mark_ir_mutated();
                return node->release_value();
            }

            return node;
        }

        std::unique_ptr<logical_ir::expression> constant_prop_pass::expression_pass::dispatch(std::unique_ptr<logical_ir::increment_operator>&& node) {
            if (std::holds_alternative<std::shared_ptr<logical_ir::variable>>(node->destination())) {
                auto& variable = std::get<std::shared_ptr<logical_ir::variable>>(node->destination());
                if (m_pass.can_remove_variable(variable)) {
                    mark_ir_mutated();
                    return node->release_destination();
                }
            }
            return node;
        }

        std::unique_ptr<logical_ir::expression> constant_prop_pass::expression_pass::dispatch(std::unique_ptr<logical_ir::variable_reference>&& node) {
            auto& variable = node->get_variable();

            std::unique_ptr<logical_ir::constant_expression> propagated_constant = m_pass.propagate_constant(variable);
            if (propagated_constant) {
                mark_ir_mutated();
                return propagated_constant->clone();
            }
            return node;
        }

        std::unique_ptr<logical_ir::statement> constant_prop_pass::statement_pass::dispatch(std::unique_ptr<logical_ir::variable_declaration>&& node) {
            auto& variable = node->variable();
            if (m_pass.can_remove_variable(variable)) {
                mark_ir_mutated();
                return nullptr;
            }
            if (m_pass.can_propagate_constant(variable)) {
                if (auto initializer = dynamic_cast<const logical_ir::constant_expression*>(node->initializer().get())) {
                    mark_ir_mutated();
                    m_pass.m_constant_expressions.insert({ variable, initializer->clone() });
                    return nullptr;
                }
            }
            return node;
        }
    }
}