#ifndef MICHAELCC_RECURSION_ANALYSIS_HPP

#include "logic/ir.hpp"
#include <memory>
#include <unordered_set>
#include <optional>
#include <queue>
#include <vector>

namespace michaelcc {
    namespace logic {
        namespace analysis {
            class recursion_analysis : public logic::const_visitor {
            private:
                std::shared_ptr<logic::function_definition> m_current_function;
                std::unordered_set<std::shared_ptr<logic::function_definition>> m_called_functions;
                
            protected:
                void visit(const logic::function_call& node) override {
                    if (std::holds_alternative<std::shared_ptr<logic::function_definition>>(node.callee())) {
                        std::shared_ptr<logic::function_definition> function = std::get<std::shared_ptr<logic::function_definition>>(node.callee());
                        m_called_functions.insert(function);
                    }
                }

            public:
                static std::unordered_set<std::shared_ptr<logic::function_definition>> called_functions(const logic::function_definition& function) {
                    std::unique_ptr<recursion_analysis> analysis = std::make_unique<recursion_analysis>();
                    function.accept(*analysis);
                    return std::move(analysis->m_called_functions);
                }
            };

            class function_dependency_analyzer {
            private:
                std::unordered_map<std::shared_ptr<logic::function_definition>, std::unordered_set<std::shared_ptr<logic::function_definition>>> m_function_dependencies;

            public:
                void analyze_function(std::shared_ptr<logic::function_definition> function) {
                    std::unordered_set<std::shared_ptr<logic::function_definition>> called_functions = recursion_analysis::called_functions(*function);
                    m_function_dependencies.insert(std::make_pair(function, std::move(called_functions)));
                }

                bool is_recursive(std::shared_ptr<logic::function_definition> function) const {
                    if (!m_function_dependencies.contains(function)) {
                        return false;
                    }
                    return m_function_dependencies.at(function).contains(function);
                }

                bool has_inline_dependencies(std::shared_ptr<logic::function_definition> function) const {
                    if (!m_function_dependencies.contains(function)) {
                        return false;
                    }
                    return std::any_of(
                        m_function_dependencies.at(function).begin(), 
                        m_function_dependencies.at(function).end(), 
                        [&](const auto& dep) { return dep->should_inline(); }
                    );
                }

                std::optional<std::vector<std::shared_ptr<logic::function_definition>>> get_mutual_recursion(std::shared_ptr<logic::function_definition> function, bool is_inlining=false) const {
                    if (!m_function_dependencies.contains(function)) return std::nullopt;

                    std::queue<std::vector<std::shared_ptr<logic::function_definition>>> queue;
                    std::unordered_set<std::shared_ptr<logic::function_definition>> visited;

                    for (const auto& dep : m_function_dependencies.at(function)) {
                        if (dep == function) return std::vector{function};
                        if (is_inlining && !dep->should_inline()) continue;
                        queue.push({function, dep});
                    }

                    while (!queue.empty()) {
                        auto path = std::move(queue.front()); queue.pop();
                        auto current = path.back();
                        if (!visited.insert(current).second) { continue; }
                        if (!m_function_dependencies.contains(current)) { continue; }

                        for (const auto& dep : m_function_dependencies.at(current)) {
                            if (dep == function) { return path; }
                            if (is_inlining && !dep->should_inline()){ continue; }
                            if (!visited.contains(dep)) { continue; }

                            auto p = path; 
                            p.push_back(dep); 
                            queue.push(std::move(p));
                        }
                    }
                    return std::nullopt;
                }
            };
        }
    }
}

#endif