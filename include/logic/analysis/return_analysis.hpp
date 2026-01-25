#ifndef MICHAELCC_OPTIMIZATION_RETURN_ANALYSIS_HPP
#define MICHAELCC_OPTIMIZATION_RETURN_ANALYSIS_HPP

#include "logic/logical.hpp"
#include <memory>

namespace michaelcc {
    namespace logical_ir {
        namespace analysis {
            class return_analysis : public logical_ir::const_statement_dispatcher<bool> {
            protected:
                bool analyze_control_block(const logical_ir::control_block& node) {
                    for (const auto& statement : node.statements()) {
                        if ((*this)(*statement)) {
                            return true;
                        }
                    }
                    return false;
                }

                bool dispatch(const logical_ir::return_statement& node) override {
                    return true;
                }

                bool dispatch(const logical_ir::statement_block& node) override {
                    return analyze_control_block(*node.control_block());
                }

                bool dispatch(const logical_ir::if_statement& node) override {
                    return analyze_control_block(*node.then_body()) && node.else_body() != nullptr && analyze_control_block(*node.else_body());
                }

                bool handle_default(const logical_ir::statement& node) override {
                    return false;
                }

            public:
                static bool all_code_paths_return(const logical_ir::control_block& node) {
                    std::unique_ptr<return_analysis> analysis = std::make_unique<return_analysis>();
                    return analysis->analyze_control_block(node);
                }
            };
        }
    }
}
#endif