#ifndef MICHAELCC_CONSTANT_PROPAGATION
#define MICHAELCC_CONSTANT_PROPAGATION

//#include "logic/dataflow.hpp"
#include "logic/dataflow.hpp"
#include "logic/logical.hpp"
#include "logic/typing.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace michaelcc {
    namespace dataflow {
        class variable_use_analyzer : public logical_ir::const_visitor{
        public:
            struct variable_metrics {
                bool is_mutated = false;
                int num_uses = 0;
            };

        private:
            std::unordered_set<const logical_ir::expression*> m_dead_expressions;
            std::unordered_map<std::shared_ptr<logical_ir::variable>, variable_metrics> m_variable_metrics;

        public:
            std::unordered_map<std::shared_ptr<logical_ir::variable>, variable_metrics> get_metrics(logical_ir::translation_unit& program) {
                m_variable_metrics = std::unordered_map<std::shared_ptr<logical_ir::variable>, variable_metrics>();
                program.accept(*this);
                return std::move(m_variable_metrics); 
            }

        protected:
            void visit(const logical_ir::variable_declaration& node) override;
            void visit(const logical_ir::set_variable& node) override;
            void visit(const logical_ir::increment_operator& node) override;
            void visit(const logical_ir::variable_reference& node) override;
            void visit(const logical_ir::address_of& node) override;
            void visit(const logical_ir::function_definition& node) override;
            void visit(const logical_ir::expression_statement& node) override;
        };

        class constant_prop_pass final : public transform_pass {
        private:
            class expression_pass : public default_expression_pass {
            private:
                constant_prop_pass& m_pass;

            public:
                expression_pass(constant_prop_pass& pass) : m_pass(pass) {}

            protected:
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::set_variable>&& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::increment_operator>&& node) override;
                std::unique_ptr<logical_ir::expression> dispatch(std::unique_ptr<logical_ir::variable_reference>&& node) override;
            };

            class statement_pass : public default_statement_pass {
            private:
                constant_prop_pass& m_pass;

            public:
                statement_pass(constant_prop_pass& pass) : m_pass(pass) {}

            protected:
                std::unique_ptr<logical_ir::statement> dispatch(std::unique_ptr<logical_ir::variable_declaration>&& node) override;
            };

            std::unordered_map<std::shared_ptr<logical_ir::variable>, variable_use_analyzer::variable_metrics> m_variable_metrics;
            std::unordered_map<std::shared_ptr<logical_ir::variable>, std::unique_ptr<logical_ir::constant_expression>> m_constant_expressions;
        protected:
            bool can_propagate_constant(const std::shared_ptr<logical_ir::variable>& variable) const {
                return m_variable_metrics.at(variable).is_mutated == false 
                    && !(variable->get_type().qualifiers() & typing::VOLATILE_TYPE_QUALIFIER);
            }

            bool can_remove_variable(const std::shared_ptr<logical_ir::variable>& variable) const {
                return m_variable_metrics.at(variable).num_uses == 0
                && !(variable->get_type().qualifiers() & typing::VOLATILE_TYPE_QUALIFIER);
            }

            std::unique_ptr<logical_ir::constant_expression> propagate_constant(const std::shared_ptr<logical_ir::variable>& variable) const {
                if (!can_propagate_constant(variable) || !m_constant_expressions.contains(variable)) {
                    return nullptr;
                }
                return m_constant_expressions.at(variable)->clone();
            }

        public:
            constant_prop_pass(logical_ir::translation_unit& program) : transform_pass(
                std::make_unique<expression_pass>(*this),
                std::make_unique<statement_pass>(*this),
                [](const std::string& name) { return name; }
            ), m_variable_metrics(std::make_unique<variable_use_analyzer>()->get_metrics(program)) {}
        };
    }
}
#endif