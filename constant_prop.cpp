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

        void variable_use_analyzer::visit(const logical_ir::set_address& node) {
            mutated_variable_tracker tracker;
            auto [mutated_variables, dead_expressions] = tracker.get_mutated_variables(*node.destination());
            for (const auto& variable : mutated_variables) {
                auto& metrics = m_variable_metrics[variable];
                metrics.is_mutated = true;
            }

            if (m_dead_expressions.contains(&node)) {
                m_dead_expressions.insert(dead_expressions.begin(), dead_expressions.end());
                return;
            }

            for (const auto& variable : mutated_variables) {
                auto& metrics = m_variable_metrics[variable];
                metrics.num_uses++;
            }
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
            if (m_dead_expressions.contains(&node)) {
                return;
            }

            auto& metrics = m_variable_metrics[node.get_variable()];
            metrics.num_uses++;
        }

        void variable_use_analyzer::visit(const logical_ir::address_of& node) {
            if (m_dead_expressions.contains(&node)) {
                return;
            }
            
            if (std::holds_alternative<std::shared_ptr<logical_ir::variable>>(node.operand())) {
                auto& metrics = m_variable_metrics[std::get<std::shared_ptr<logical_ir::variable>>(node.operand())];
                metrics.num_uses++;
                metrics.is_mutated = true;
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

        std::unique_ptr<logical_ir::expression> constant_prop_pass::expression_pass::dispatch(std::unique_ptr<logical_ir::set_address>&& node) {
            variable_use_analyzer::mutated_variable_tracker tracker;
            auto [mutated_variables, dead_expressions] = tracker.get_mutated_variables(*node->destination());

            for (const auto& variable : mutated_variables) {
                if (!m_pass.can_remove_variable(variable)) {
                    return node;
                }
            }

            mark_ir_mutated();

            dest_side_effects side_effects;
            auto statements = side_effects.get_statements(*node->destination());

            if (statements.empty()) {
                return node->release_value();
            }
            else {
                std::shared_ptr<logical_ir::control_block> control_block = std::make_shared<logical_ir::control_block>();
                control_block->implement(std::move(statements));
                return std::make_unique<logical_ir::compound_expression>(std::move(control_block), std::move(node->release_value()));
            }
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

            std::unique_ptr<logical_ir::expression> propagated_constant = m_pass.propagate_constant(variable);
            if (propagated_constant) {
                mark_ir_mutated();
                return propagated_constant;
            }
            return node;
        }

        std::unique_ptr<logical_ir::statement> constant_prop_pass::statement_pass::dispatch(std::unique_ptr<logical_ir::variable_declaration>&& node) {
            auto& variable = node->variable();
            if (m_pass.can_remove_variable(variable)) {
                mark_ir_mutated();
                return std::make_unique<logical_ir::expression_statement>(node->release_initializer());
            }
            if (m_pass.can_propagate_constant(variable)) {
                constant_cloner cloner;
                auto initializer = cloner(*node->initializer());
                if (!initializer) {
                    return node;
                }
                
                mark_ir_mutated();
                m_pass.m_constant_expressions.insert({ variable, std::move(initializer) });
                return nullptr;
            }
            return node;
        }
    }
}