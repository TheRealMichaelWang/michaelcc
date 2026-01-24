#ifndef MICHAELCC_ANALYSIS_SIDE_EFFECTS_HPP
#define MICHAELCC_ANALYSIS_SIDE_EFFECTS_HPP

#include "logic/logical.hpp"

namespace michaelcc {
    namespace logical_ir {
        namespace analysis {
            class side_effects_analyzer {
            public:
                bool expression_has_side_effects(const logical_ir::expression& expression);
                bool statement_has_side_effects(const logical_ir::statement& statement);
                bool control_block_has_side_effects(const logical_ir::control_block& control_block);

            private:
                class expression_analyzer : public logical_ir::const_expression_dispatcher<bool> {
                private:
                    side_effects_analyzer& m_side_effects_analyzer;
                
                    public:
                    expression_analyzer(side_effects_analyzer& side_effects_analyzer) : m_side_effects_analyzer(side_effects_analyzer) {}

                protected:
                    bool dispatch(const logical_ir::integer_constant& node) override { return false; }
                    bool dispatch(const logical_ir::floating_constant& node) override { return false; }
                    bool dispatch(const logical_ir::string_constant& node) override { return false; }
                    bool dispatch(const logical_ir::enumerator_literal& node) override { return false; }

                    bool dispatch(const logical_ir::variable_reference& node) override { return false; }
                    bool dispatch(const logical_ir::increment_operator& node) override { return true; }
                    bool dispatch(const logical_ir::arithmetic_operator& node) override { return (*this)(*node.left()) || (*this)(*node.right()); }
                    bool dispatch(const logical_ir::unary_operation& node) override { return (*this)(*node.operand()); }
                    bool dispatch(const logical_ir::type_cast& node) override { return (*this)(*node.operand()); }
                    bool dispatch(const logical_ir::dereference& node) override { return (*this)(*node.operand()); }
                    bool dispatch(const logical_ir::member_access& node) override { return (*this)(*node.base()); }
                    bool dispatch(const logical_ir::array_index& node) override { return (*this)(*node.base()) || (*this)(*node.index()); }

                    bool dispatch(const logical_ir::address_of& node) override {
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

                    bool dispatch(const logical_ir::conditional_expression& node) override { return (*this)(*node.condition()) || (*this)(*node.then_expression()) || (*this)(*node.else_expression()); }
                    bool dispatch(const logical_ir::set_address& node) override { return true; }
                    bool dispatch(const logical_ir::set_variable& node) override { return true; }
                    bool dispatch(const logical_ir::function_reference& node) override { return false; }

                    bool dispatch(const logical_ir::array_initializer& node) override { 
                        return std::any_of(node.initializers().begin(), node.initializers().end(), [&](const auto& initializer) { return (*this)(*initializer); }); 
                    }
                    
                    bool dispatch(const logical_ir::allocate_array& node) override { 
                        return std::any_of(node.dimensions().begin(), node.dimensions().end(), [&](const auto& dimension) { return (*this)(*dimension); }); 
                    }
                        
                    bool dispatch(const logical_ir::struct_initializer& node) override { 
                        return std::any_of(node.initializers().begin(), node.initializers().end(), [&](const auto& initializer) { return (*this)(*initializer.initializer); }); 
                    }
                    
                    bool dispatch(const logical_ir::union_initializer& node) override { return (*this)(*node.initializer()); }
                    bool dispatch(const logical_ir::function_call& node) override { return true; }

                    bool dispatch(const logical_ir::compound_expression& node) override { 
                        return m_side_effects_analyzer.expression_has_side_effects(*node.return_expression()) || m_side_effects_analyzer.control_block_has_side_effects(*node.control_block());
                    }
                };
                
                class statement_analyzer : public logical_ir::const_statement_dispatcher<bool> {
                private:
                    side_effects_analyzer& m_side_effects_analyzer;

                public:
                    statement_analyzer(side_effects_analyzer& side_effects_analyzer) : m_side_effects_analyzer(side_effects_analyzer) {}

                    bool control_block_analyzer(const logical_ir::control_block& node) {
                        for (const auto& statement : node.statements()) {
                            if ((*this)(*statement)) {
                                return true;
                            }
                        }
                        return false;
                    }

                protected:
                    bool dispatch(const logical_ir::statement_block& node) override { return control_block_analyzer(*node.control_block()); }
                    bool dispatch(const logical_ir::if_statement& node) override { return control_block_analyzer(*node.then_body()) && node.else_body() != nullptr && control_block_analyzer(*node.else_body()); }
                    bool dispatch(const logical_ir::loop_statement& node) override { return control_block_analyzer(*node.body()); }
                    bool dispatch(const logical_ir::break_statement& node) override { return false; }
                    bool dispatch(const logical_ir::continue_statement& node) override { return false; }
                    bool dispatch(const logical_ir::return_statement& node) override { return true; }

                    bool dispatch(const logical_ir::expression_statement& node) override { 
                        return node.expression() != nullptr && m_side_effects_analyzer.expression_has_side_effects(*node.expression()); 
                    }
                    
                    bool dispatch(const logical_ir::variable_declaration& node) override { return false; }
                };

                expression_analyzer m_expression_analyzer;
                statement_analyzer m_statement_analyzer;
            };

            bool side_effects_analyzer::expression_has_side_effects(const logical_ir::expression& expression) {
                return this->m_expression_analyzer(expression);
            }

            bool side_effects_analyzer::statement_has_side_effects(const logical_ir::statement& statement) {
                return this->m_statement_analyzer(statement);
            }

            bool side_effects_analyzer::control_block_has_side_effects(const logical_ir::control_block& control_block) {
                return this->m_statement_analyzer.control_block_analyzer(control_block);
            }
        }
    }
}
#endif