#include "logic/optimization/ir_simplify.hpp"
#include "logic/ir.hpp"
#include <memory>
#include <variant>

namespace michaelcc {
    namespace optimization {
        std::unique_ptr<logic::expression> ir_simplify_pass::expression_pass::dispatch(std::unique_ptr<logic::set_address>&& node) {
            if (auto address_of = dynamic_cast<logic::address_of*>(node->destination().get())) {
                if (std::holds_alternative<std::shared_ptr<logic::variable>>(address_of->operand())) {
                    mark_ir_mutated();
                    std::shared_ptr<logic::variable> variable = std::get<std::shared_ptr<logic::variable>>(address_of->operand());
                    return std::make_unique<logic::set_variable>(std::move(variable), node->release_value());
                }
            }
            return node;
        }

        std::unique_ptr<logic::expression> ir_simplify_pass::expression_pass::dispatch(std::unique_ptr<logic::dereference>&& node) {
            if (auto address_of = dynamic_cast<logic::address_of*>(node->operand().get())) {
                mark_ir_mutated();
                return address_of->release_operand();
            }

            if (auto arithmetic_operator = dynamic_cast<logic::arithmetic_operator*>(node->operand().get())) {
                if (arithmetic_operator->get_operator() == MICHAELCC_TOKEN_PLUS) {
                    if (arithmetic_operator->left()->get_type().is_same_type<typing::pointer_type>() && arithmetic_operator->right()->get_type().is_same_type<typing::int_type>()) {
                        mark_ir_mutated();
                        return std::make_unique<logic::array_index>(arithmetic_operator->release_left(), arithmetic_operator->release_right());
                    }
                    if (arithmetic_operator->left()->get_type().is_same_type<typing::int_type>() && arithmetic_operator->right()->get_type().is_same_type<typing::pointer_type>()) {
                        mark_ir_mutated();
                        return std::make_unique<logic::array_index>(arithmetic_operator->release_right(), arithmetic_operator->release_left());
                    }
                }
            }
            return node;
        }

        std::unique_ptr<logic::expression> ir_simplify_pass::expression_pass::dispatch(std::unique_ptr<logic::compound_expression>&& node) {
            if (node->control_block()->statements().empty()) {
                return node->release_return_expression();
            }
            return node;
        }

        std::unique_ptr<logic::expression> ir_simplify_pass::expression_pass::dispatch(std::unique_ptr<logic::member_access>&& node) {
            if (node->is_dereference()) {
                if (auto address_of = dynamic_cast<logic::address_of*>(node->base().get())) {
                    mark_ir_mutated();
                    return std::make_unique<logic::member_access>(address_of->release_operand(), node->member(), false);
                }
            }
            else {
                if (auto struct_initializer = dynamic_cast<logic::struct_initializer*>(node->base().get())) {
                    std::vector<logic::struct_initializer::member_initializer> initializers = struct_initializer->release_initializers();
                    for (auto& initializer : initializers) {
                        if (initializer.member_name == node->member().name) {
                            mark_ir_mutated();
                            return std::move(initializer.initializer);
                        }
                    }
                    return node;
                }

                if (auto union_initializer = dynamic_cast<logic::union_initializer*>(node->base().get())) {
                    if (union_initializer->target_member().name == node->member().name) {
                        mark_ir_mutated();
                        return union_initializer->release_initializer();
                    }
                    return node;
                }
            }
            return node;
        }

        std::unique_ptr<logic::expression> ir_simplify_pass::expression_pass::dispatch(std::unique_ptr<logic::array_index>&& node) {
            if (auto array_initializer = dynamic_cast<logic::array_initializer*>(node->base().get())) {
                if (auto index = dynamic_cast<logic::integer_constant*>(node->index().get())) {
                    std::vector<std::unique_ptr<logic::expression>> initializers = array_initializer->release_initializers();
                    if (index->value() >= 0 && index->value() < initializers.size()) {
                        mark_ir_mutated();
                        return std::move(initializers[index->value()]);
                    }
                }
            }
            return node;
        }
    }
}