#ifndef MICHAELCC_CONSTANT_ANALYSIS_HPP
#define MICHAELCC_CONSTANT_ANALYSIS_HPP

#include "logic/ir.hpp"
#include "platform.hpp"
#include <memory>

namespace michaelcc {
    namespace logic {
        namespace analysis {
            class constant_cloner final : public logic::const_expression_dispatcher<std::unique_ptr<logic::expression>> {
            private:
                const platform_info m_platform_info;
    
            public:
                constant_cloner(const platform_info platform_info) : m_platform_info(platform_info) {}
    
            protected:
                std::unique_ptr<logic::expression> dispatch(const logic::integer_constant& node) override { 
                    return std::make_unique<logic::integer_constant>(node.value(), typing::qual_type(node.get_type())); 
                }
    
                std::unique_ptr<logic::expression> dispatch(const logic::floating_constant& node) override { 
                    return std::make_unique<logic::floating_constant>(node.value(), typing::qual_type(node.get_type())); 
                }
    
                std::unique_ptr<logic::expression> dispatch(const logic::string_constant& node) override { 
                    return std::make_unique<logic::string_constant>(node.index()); 
                }
    
                std::unique_ptr<logic::expression> dispatch(const logic::enumerator_literal& node) override { 
                    return std::make_unique<logic::enumerator_literal>(typing::enum_type::enumerator(node.enumerator()), std::shared_ptr<typing::enum_type>(node.enum_type())); 
                }
    
                std::unique_ptr<logic::expression> dispatch(const logic::array_initializer& node) override { 
                    std::vector<std::unique_ptr<logic::expression>> initializers;
                    initializers.reserve(node.initializers().size());
                    for (const auto& initializer : node.initializers()) {
                        auto element = (*this)(*initializer);
                        if (!element || !node.element_type().is_assignable_from(element->get_type(), m_platform_info)) {
                            return nullptr;
                        }
    
                        initializers.emplace_back(std::move(element));
                    }
                    return std::make_unique<logic::array_initializer>(std::move(initializers), typing::qual_type(node.element_type()));
                }
                
                std::unique_ptr<logic::expression> dispatch(const logic::struct_initializer& node) override { 
                    std::vector<logic::struct_initializer::member_initializer> initializers;
                    initializers.reserve(node.initializers().size());
                    for (size_t i = 0; i < node.initializers().size(); i++) {
                        auto element = (*this)(*node.initializers()[i].initializer);
                        if (!element || !node.struct_type()->fields()[i].member_type.is_assignable_from(element->get_type(), m_platform_info)) {
                            return nullptr;
                        }
    
                        initializers.emplace_back(logic::struct_initializer::member_initializer{node.initializers()[i].member_name, std::move(element)});
                    }
                    return std::make_unique<logic::struct_initializer>(std::move(initializers), std::shared_ptr<typing::struct_type>(node.struct_type()));
                }
    
                std::unique_ptr<logic::expression> dispatch(const logic::union_initializer& node) override { 
                    auto element = (*this)(*node.initializer());
                    if (!element || !node.target_member().member_type.is_assignable_from(element->get_type(), m_platform_info)) {
                        return nullptr;
                    }
                    return std::make_unique<logic::union_initializer>(std::move(element), std::shared_ptr<typing::union_type>(node.union_type()), typing::member(node.target_member()));
                }
    
                std::unique_ptr<logic::expression> dispatch(const logic::type_cast& node) override { 
                    auto expression = (*this)(*node.operand());
                    if (!expression) {
                        return nullptr;
                    }
                    return std::make_unique<logic::type_cast>(std::move(expression), typing::qual_type(node.get_type()));
                }
    
                std::unique_ptr<logic::expression> handle_default(const logic::expression& node) override { return nullptr; }
            };

            inline bool is_constant(const logic::expression& expression, const platform_info& platform_info) {
                constant_cloner cloner(platform_info);
                return cloner(expression) != nullptr;
            }
        }
    }
}

#endif