#ifndef MICHAELCC_CONSTANT_PROPAGATION
#define MICHAELCC_CONSTANT_PROPAGATION

//#include "logic/dataflow.hpp"
#include "logic/logical.hpp"
#include <unordered_map>

namespace michaelcc {
    namespace dataflow {
        namespace constant_prop {
            class variable_use_analyzer : public logical_ir::const_visitor{
            public:
                struct variable_metrics {
                    bool is_mutated = false;
                    int num_uses = 0;
                };

            private:
                std::unordered_map<std::shared_ptr<logical_ir::variable>, variable_metrics> m_variable_metrics;

            protected:
                void visit(const logical_ir::variable_declaration& node) override;
                void visit(const logical_ir::set_variable& node) override;
                void visit(const logical_ir::increment_operator& node) override;
                void visit(const logical_ir::expression_statement& node) override;
                void visit(const logical_ir::variable_reference& node) override;
                void visit(const logical_ir::address_of& node) override;
            };
        }
    }
}
#endif