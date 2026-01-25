#include "logic/dataflow/constant_folding.hpp"
#include "logic/logical.hpp"
#include "logic/typing.hpp"
#include "logic/semantic.hpp"

namespace michaelcc {
    namespace dataflow {

        // Helper to fold float arithmetic operations
        static std::unique_ptr<logical_ir::expression> fold_float_arithmetic(
            token_type op, double left_val, double right_val, typing::qual_type result_type
        ) {
            double result_val;
            switch (op) {
                case MICHAELCC_TOKEN_PLUS:        result_val = left_val + right_val; break;
                case MICHAELCC_TOKEN_MINUS:       result_val = left_val - right_val; break;
                case MICHAELCC_TOKEN_ASTERISK:    result_val = left_val * right_val; break;
                case MICHAELCC_TOKEN_SLASH:       result_val = left_val / right_val; break;
                case MICHAELCC_TOKEN_DOUBLE_AND:  result_val = (left_val != 0.0 && right_val != 0.0) ? 1 : 0; break;
                case MICHAELCC_TOKEN_DOUBLE_OR:   result_val = (left_val != 0.0 || right_val != 0.0) ? 1 : 0; break;
                case MICHAELCC_TOKEN_EQUALS:      result_val = (left_val == right_val) ? 1 : 0; break;
                case MICHAELCC_TOKEN_NOT_EQUALS:  result_val = (left_val != right_val) ? 1 : 0; break;
                case MICHAELCC_TOKEN_LESS:        result_val = (left_val < right_val) ? 1 : 0; break;
                case MICHAELCC_TOKEN_MORE:        result_val = (left_val > right_val) ? 1 : 0; break;
                case MICHAELCC_TOKEN_LESS_EQUAL:  result_val = (left_val <= right_val) ? 1 : 0; break;
                case MICHAELCC_TOKEN_MORE_EQUAL:  result_val = (left_val >= right_val) ? 1 : 0; break;
                default: return nullptr;
            }

            if (result_type.is_same_type<typing::int_type>()) {
                return std::make_unique<logical_ir::integer_constant>(static_cast<uint64_t>(result_val), std::move(result_type));
            }
            return std::make_unique<logical_ir::floating_constant>(result_val, std::move(result_type));
        }

        std::unique_ptr<logical_ir::expression> constant_folding_pass::expression_pass::dispatch(std::unique_ptr<logical_ir::arithmetic_operator>&& node) {
            auto* left_int = dynamic_cast<logical_ir::integer_constant*>(node->left().get());
            auto* right_int = dynamic_cast<logical_ir::integer_constant*>(node->right().get());
            auto* left_float = dynamic_cast<logical_ir::floating_constant*>(node->left().get());
            auto* right_float = dynamic_cast<logical_ir::floating_constant*>(node->right().get());

            auto mode = semantic_lowerer::get_arbitration_mode(node->get_operator());

            // Integer + Integer
            if (left_int && right_int) {
                typing::int_type* left_int_type = static_cast<typing::int_type*>(left_int->get_type().type().get());
                typing::int_type* right_int_type = static_cast<typing::int_type*>(right_int->get_type().type().get());

                uint64_t left_val = left_int->value();
                uint64_t right_val = right_int->value();
                uint64_t result_val = 0;
                
                auto result_type = semantic_lowerer::arbitrate_return_type(left_int->get_type(), right_int->get_type(), m_layout_calculator.get_platform_info(), mode);
                if (!result_type) return node;

                // Shift operations use left operand type per C semantics
                if (node->get_operator() == MICHAELCC_TOKEN_BITSHIFT_LEFT ||
                    node->get_operator() == MICHAELCC_TOKEN_BITSHIFT_RIGHT) {
                    result_type = typing::qual_type(left_int->get_type());
                }

                switch (node->get_operator()) {
                    case MICHAELCC_TOKEN_PLUS:
                        result_val = left_val + right_val;
                        break;
                    case MICHAELCC_TOKEN_MINUS:
                        result_val = left_val - right_val;
                        break;
                    case MICHAELCC_TOKEN_ASTERISK:
                        result_val = left_val * right_val;
                        break;
                    case MICHAELCC_TOKEN_SLASH:
                        if (right_val == 0) return node;
                        if (left_int_type->is_unsigned() || right_int_type->is_unsigned()) {
                            result_val = left_val / right_val;
                        } else {
                            result_val = static_cast<uint64_t>(static_cast<int64_t>(left_val) / static_cast<int64_t>(right_val));
                        }
                        break;
                    case MICHAELCC_TOKEN_MODULO:
                        if (right_val == 0) return node;
                        if (left_int_type->is_unsigned() || right_int_type->is_unsigned()) {
                            result_val = left_val % right_val;
                        } else {
                            result_val = static_cast<uint64_t>(static_cast<int64_t>(left_val) % static_cast<int64_t>(right_val));
                        }
                        break;
                    case MICHAELCC_TOKEN_AND:
                        result_val = left_val & right_val;
                        break;
                    case MICHAELCC_TOKEN_OR:
                        result_val = left_val | right_val;
                        break;
                    case MICHAELCC_TOKEN_CARET:
                        result_val = left_val ^ right_val;
                        break;
                    case MICHAELCC_TOKEN_BITSHIFT_LEFT:
                    case MICHAELCC_TOKEN_BITSHIFT_RIGHT: {
                        // Compute bit width of left operand type to avoid UB
                        // Shifting by >= bit width is undefined behavior in C
                        const uint64_t bit_width = m_layout_calculator(*left_int_type).size * 8;
                        
                        // Don't fold if shift amount >= bit width (UB in C)
                        if (right_val >= bit_width) {
                            return node;
                        }
                        
                        if (node->get_operator() == MICHAELCC_TOKEN_BITSHIFT_LEFT) {
                            result_val = left_val << right_val;
                        } else {
                            // MICHAELCC_TOKEN_BITSHIFT_RIGHT
                            if (left_int_type->is_unsigned()) {
                                result_val = left_val >> right_val;
                            } else {
                                result_val = static_cast<uint64_t>(static_cast<int64_t>(left_val) >> right_val);
                            }
                        }
                        break;
                    }
                    case MICHAELCC_TOKEN_DOUBLE_AND:
                        result_val = (left_val != 0 && right_val != 0) ? 1 : 0;
                        break;
                    case MICHAELCC_TOKEN_DOUBLE_OR:
                        result_val = (left_val != 0 || right_val != 0) ? 1 : 0;
                        break;
                    case MICHAELCC_TOKEN_EQUALS:
                        result_val = (left_val == right_val) ? 1 : 0;
                        break;
                    case MICHAELCC_TOKEN_NOT_EQUALS:
                        result_val = (left_val != right_val) ? 1 : 0;
                        break;
                    case MICHAELCC_TOKEN_LESS:
                        if (left_int_type->is_unsigned() && right_int_type->is_unsigned()) {
                            result_val = (left_val < right_val) ? 1 : 0;
                        } else {
                            result_val = (static_cast<int64_t>(left_val) < static_cast<int64_t>(right_val)) ? 1 : 0;
                        }
                        break;
                    case MICHAELCC_TOKEN_MORE:
                        if (left_int_type->is_unsigned() && right_int_type->is_unsigned()) {
                            result_val = (left_val > right_val) ? 1 : 0;
                        } else {
                            result_val = (static_cast<int64_t>(left_val) > static_cast<int64_t>(right_val)) ? 1 : 0;
                        }
                        break;
                    case MICHAELCC_TOKEN_LESS_EQUAL:
                        if (left_int_type->is_unsigned() && right_int_type->is_unsigned()) {
                            result_val = (left_val <= right_val) ? 1 : 0;
                        } else {
                            result_val = (static_cast<int64_t>(left_val) <= static_cast<int64_t>(right_val)) ? 1 : 0;
                        }
                        break;
                    case MICHAELCC_TOKEN_MORE_EQUAL:
                        if (left_int_type->is_unsigned() && right_int_type->is_unsigned()) {
                            result_val = (left_val >= right_val) ? 1 : 0;
                        } else {
                            result_val = (static_cast<int64_t>(left_val) >= static_cast<int64_t>(right_val)) ? 1 : 0;
                        }
                        break;
                    default:
                        return node;
                }

                mark_ir_mutated();
                return std::make_unique<logical_ir::integer_constant>(
                    result_val,
                    std::move(*result_type)
                );
            }

            // Float + Float
            if (left_float && right_float) {
                auto result_type = semantic_lowerer::arbitrate_return_type(left_float->get_type(), right_float->get_type(), m_layout_calculator.get_platform_info(), mode);
                if (!result_type) return node;

                auto result = fold_float_arithmetic(
                    node->get_operator(),
                    left_float->value(),
                    right_float->value(),
                    std::move(*result_type)
                );
                if (result) {
                    mark_ir_mutated();
                }
                return result ? std::move(result) : std::move(node);
            }

            // Float + Int (promote int to float)
            if (left_float && right_int) {
                typing::int_type* int_type = static_cast<typing::int_type*>(right_int->get_type().type().get());

                auto result_type = semantic_lowerer::arbitrate_return_type(left_float->get_type(), right_int->get_type(), m_layout_calculator.get_platform_info(), mode);
                if (!result_type) return node;

                double right_val = int_type->is_unsigned() 
                    ? static_cast<double>(right_int->value())
                    : static_cast<double>(static_cast<int64_t>(right_int->value()));

                auto result = fold_float_arithmetic(
                    node->get_operator(),
                    left_float->value(),
                    right_val,
                    std::move(*result_type)
                );
                if (result) {
                    mark_ir_mutated();
                }
                return result ? std::move(result) : std::move(node);
            }

            // Int + Float (promote int to float)
            if (left_int && right_float) {
                typing::int_type* int_type = static_cast<typing::int_type*>(left_int->get_type().type().get());

                auto result_type = semantic_lowerer::arbitrate_return_type(left_int->get_type(), right_float->get_type(), m_layout_calculator.get_platform_info(), mode);
                if (!result_type) return node;

                double left_val = int_type->is_unsigned()
                    ? static_cast<double>(left_int->value())
                    : static_cast<double>(static_cast<int64_t>(left_int->value()));

                auto result = fold_float_arithmetic(
                    node->get_operator(),
                    left_val,
                    right_float->value(),
                    std::move(*result_type)
                );
                if (result) {
                    mark_ir_mutated();
                }
                return result ? std::move(result) : std::move(node);
            }

            return node;
        }

        std::unique_ptr<logical_ir::expression> constant_folding_pass::expression_pass::dispatch(std::unique_ptr<logical_ir::unary_operation>&& node) {
            const auto& operand = node->operand();

            logical_ir::integer_constant* integer_constant = dynamic_cast<logical_ir::integer_constant*>(operand.get());
            if (integer_constant) {
                typing::int_type* int_type = static_cast<typing::int_type*>(integer_constant->get_type().type().get());

                switch (node->get_operator()) {
                    case MICHAELCC_TOKEN_MINUS:
                        if (int_type->is_unsigned()) {
                            return node;
                        }

                        mark_ir_mutated();
                        return std::make_unique<logical_ir::integer_constant>(
                            -integer_constant->value(), 
                            typing::qual_type(integer_constant->get_type())
                        );
                    case MICHAELCC_TOKEN_NOT:
                        mark_ir_mutated();
                        return std::make_unique<logical_ir::integer_constant>(
                            integer_constant->value() == 0 ? 1 : 0, 
                            typing::qual_type(integer_constant->get_type())
                        );
                    case MICHAELCC_TOKEN_TILDE:
                        mark_ir_mutated();
                        return std::make_unique<logical_ir::integer_constant>(
                            ~integer_constant->value(), 
                            typing::qual_type(integer_constant->get_type())
                        );
                    default: {
                        return node;
                    }
                }
            }

            logical_ir::floating_constant* floating_constant = dynamic_cast<logical_ir::floating_constant*>(operand.get());
            if (floating_constant) {
                switch (node->get_operator()) {
                case MICHAELCC_TOKEN_MINUS:
                    mark_ir_mutated();
                    return std::make_unique<logical_ir::floating_constant>(
                        -floating_constant->value(), 
                        typing::qual_type(floating_constant->get_type())
                    );
                default:
                    return node;
                }
            }

            return node;
        }

        std::unique_ptr<logical_ir::expression> constant_folding_pass::expression_pass::dispatch(std::unique_ptr<logical_ir::type_cast>&& node) {
            const auto& operand = node->operand();
            const auto& target_type = node->get_type();

            if (target_type.is_equivalent(operand->get_type(), m_layout_calculator.get_platform_info())) {
                mark_ir_mutated();
                return node->release_operand();
            }

            // Integer constant being cast
            if (const auto* int_const = dynamic_cast<const logical_ir::integer_constant*>(operand.get())) {
                const auto* int_type = dynamic_cast<const typing::int_type*>(int_const->get_type().type().get());
                
                // Cast to integer type
                if (target_type.is_same_type<typing::int_type>()) {
                    mark_ir_mutated();
                    return std::make_unique<logical_ir::integer_constant>(
                        int_const->value(),
                        typing::qual_type(target_type)
                    );
                }
                
                // Cast to float type
                if (target_type.is_same_type<typing::float_type>()) {
                    double value = int_type->is_unsigned()
                        ? static_cast<double>(int_const->value())
                        : static_cast<double>(static_cast<int64_t>(int_const->value()));
                    mark_ir_mutated();
                    return std::make_unique<logical_ir::floating_constant>(
                        value,
                        typing::qual_type(target_type)
                    );
                }
            }

            // Float constant being cast
            if (const auto* float_const = dynamic_cast<const logical_ir::floating_constant*>(operand.get())) {
                // Cast to integer type
                if (target_type.is_same_type<typing::int_type>()) {
                    const auto* target_int_type = static_cast<const typing::int_type*>(target_type.type().get());
                    uint64_t value = target_int_type->is_unsigned()
                        ? static_cast<uint64_t>(float_const->value())
                        : static_cast<uint64_t>(static_cast<int64_t>(float_const->value()));
                    mark_ir_mutated();
                    return std::make_unique<logical_ir::integer_constant>(
                        value,
                        typing::qual_type(target_type)
                    );
                }
                
                // Cast to float type
                if (target_type.is_same_type<typing::float_type>()) {
                    mark_ir_mutated();
                    return std::make_unique<logical_ir::floating_constant>(
                        float_const->value(),
                        typing::qual_type(target_type)
                    );
                }
            }

            return node;
        }
    }
}
