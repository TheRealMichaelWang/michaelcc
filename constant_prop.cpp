#include "logic/dataflow/constant_prop.hpp"
#include "logic/logical.hpp"
#include <memory>
#include <variant>

namespace michaelcc {
    namespace dataflow {
        void variable_use_analyzer::visit(const logical_ir::variable_declaration& node) {
            m_variable_metrics.insert({ 
                node.variable(), 
                variable_metrics{ false, 0 } 
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
                    variable_metrics{ true, 1 } 
                });
            }
        }

        void variable_use_analyzer::visit(const logical_ir::expression_statement& node) {
            m_dead_expressions.insert(node.expression().get());
        }

        namespace constant_prop {
            
        }
    }
}