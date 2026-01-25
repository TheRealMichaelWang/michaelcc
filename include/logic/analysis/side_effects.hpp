#ifndef MICHAELCC_ANALYSIS_SIDE_EFFECTS_HPP
#define MICHAELCC_ANALYSIS_SIDE_EFFECTS_HPP

#include "logic/ir.hpp"

namespace michaelcc {
    namespace logic {
        namespace analysis {
            class side_effects_analyzer {
            public:
                bool expression_has_side_effects(const logic::expression& expression);
                bool statement_has_side_effects(const logic::statement& statement);
                bool control_block_has_side_effects(const logic::control_block& control_block);

            private:
                class expression_analyzer : public logic::const_expression_dispatcher<bool> {
                private:
                    side_effects_analyzer& m_side_effects_analyzer;
                
                    public:
                    expression_analyzer(side_effects_analyzer& side_effects_analyzer) : m_side_effects_analyzer(side_effects_analyzer) {}

                protected:
                    bool dispatch(const logic::integer_constant& node) override { return false; }
                    bool dispatch(const logic::floating_constant& node) override { return false; }
                    bool dispatch(const logic::string_constant& node) override { return false; }
                    bool dispatch(const logic::enumerator_literal& node) override { return false; }

                    bool dispatch(const logic::variable_reference& node) override { return false; }
                    bool dispatch(const logic::increment_operator& node) override { return true; }
                    bool dispatch(const logic::arithmetic_operator& node) override { return (*this)(*node.left()) || (*this)(*node.right()); }
                    bool dispatch(const logic::unary_operation& node) override { return (*this)(*node.operand()); }
                    bool dispatch(const logic::type_cast& node) override { return (*this)(*node.operand()); }
                    bool dispatch(const logic::dereference& node) override { return (*this)(*node.operand()); }
                    bool dispatch(const logic::member_access& node) override { return (*this)(*node.base()); }
                    bool dispatch(const logic::array_index& node) override { return (*this)(*node.base()) || (*this)(*node.index()); }

                    bool dispatch(const logic::address_of& node) override {
                        return std::visit(overloaded{
                            [&](const std::shared_ptr<variable>&) -> bool {
                                return false;
                            },
                            [&](const std::unique_ptr<array_index>& array_index) -> bool {
                                return (*this)(*array_index);
                            },
                            [&](const std::unique_ptr<member_access>& member_access) -> bool {
                                return (*this)(*member_access);
                            }
                        }, node.operand());
                    }

                    bool dispatch(const logic::conditional_expression& node) override { return (*this)(*node.condition()) || (*this)(*node.then_expression()) || (*this)(*node.else_expression()); }
                    bool dispatch(const logic::set_address& node) override { return true; }
                    bool dispatch(const logic::set_variable& node) override { return true; }
                    bool dispatch(const logic::function_reference& node) override { return false; }

                    bool dispatch(const logic::array_initializer& node) override { 
                        return std::any_of(node.initializers().begin(), node.initializers().end(), [&](const auto& initializer) { return (*this)(*initializer); }); 
                    }
                    
                    bool dispatch(const logic::allocate_array& node) override { 
                        return std::any_of(node.dimensions().begin(), node.dimensions().end(), [&](const auto& dimension) { return (*this)(*dimension); }); 
                    }
                        
                    bool dispatch(const logic::struct_initializer& node) override { 
                        return std::any_of(node.initializers().begin(), node.initializers().end(), [&](const auto& initializer) { return (*this)(*initializer.initializer); }); 
                    }
                    
                    bool dispatch(const logic::union_initializer& node) override { return (*this)(*node.initializer()); }
                    bool dispatch(const logic::function_call& node) override { return true; }

                    bool dispatch(const logic::compound_expression& node) override { 
                        return m_side_effects_analyzer.expression_has_side_effects(*node.return_expression()) || m_side_effects_analyzer.control_block_has_side_effects(*node.control_block());
                    }
                };
                
                class statement_analyzer : public logic::const_statement_dispatcher<bool> {
                private:
                    side_effects_analyzer& m_side_effects_analyzer;

                public:
                    statement_analyzer(side_effects_analyzer& side_effects_analyzer) : m_side_effects_analyzer(side_effects_analyzer) {}

                    bool control_block_analyzer(const logic::control_block& node) {
                        for (const auto& statement : node.statements()) {
                            if ((*this)(*statement)) {
                                return true;
                            }
                        }
                        return false;
                    }

                protected:
                    bool dispatch(const logic::statement_block& node) override { return control_block_analyzer(*node.control_block()); }
                    bool dispatch(const logic::if_statement& node) override { return control_block_analyzer(*node.then_body()) || (node.else_body() != nullptr && control_block_analyzer(*node.else_body())); }
                    bool dispatch(const logic::loop_statement& node) override { return control_block_analyzer(*node.body()); }
                    bool dispatch(const logic::break_statement& node) override { return false; }
                    bool dispatch(const logic::continue_statement& node) override { return false; }
                    bool dispatch(const logic::return_statement& node) override { return true; }

                    bool dispatch(const logic::expression_statement& node) override { 
                        return node.expression() != nullptr && m_side_effects_analyzer.expression_has_side_effects(*node.expression()); 
                    }
                    
                    bool dispatch(const logic::variable_declaration& node) override { return false; }
                };

                expression_analyzer m_expression_analyzer;
                statement_analyzer m_statement_analyzer;

            public:
                side_effects_analyzer() : m_expression_analyzer(*this), m_statement_analyzer(*this) {}
            };

            inline bool side_effects_analyzer::expression_has_side_effects(const logic::expression& expression) {
                return this->m_expression_analyzer(expression);
            }

            inline bool side_effects_analyzer::statement_has_side_effects(const logic::statement& statement) {
                return this->m_statement_analyzer(statement);
            }

            inline bool side_effects_analyzer::control_block_has_side_effects(const logic::control_block& control_block) {
                return this->m_statement_analyzer.control_block_analyzer(control_block);
            }
        }
    }
}
#endif