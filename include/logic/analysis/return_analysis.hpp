#ifndef MICHAELCC_OPTIMIZATION_RETURN_ANALYSIS_HPP
#define MICHAELCC_OPTIMIZATION_RETURN_ANALYSIS_HPP

#include "logic/ir.hpp"
#include <memory>

namespace michaelcc {
    namespace logic {
        namespace analysis {
            class return_analysis : public logic::const_statement_dispatcher<bool> {
            protected:
                bool analyze_control_block(const logic::control_block& node) {
                    for (const auto& statement : node.statements()) {
                        if ((*this)(*statement)) {
                            return true;
                        }
                    }
                    return false;
                }

                bool dispatch(const logic::return_statement& node) override {
                    return true;
                }

                bool dispatch(const logic::statement_block& node) override {
                    return analyze_control_block(*node.control_block());
                }

                bool dispatch(const logic::if_statement& node) override {
                    return analyze_control_block(*node.then_body()) && node.else_body() != nullptr && analyze_control_block(*node.else_body());
                }

                bool handle_default(const logic::statement& node) override {
                    return false;
                }

            public:
                static bool all_code_paths_return(const logic::control_block& node) {
                    std::unique_ptr<return_analysis> analysis = std::make_unique<return_analysis>();
                    return analysis->analyze_control_block(node);
                }
            };
        }
    }
}
#endif