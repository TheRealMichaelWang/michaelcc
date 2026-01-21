#include "logic/dataflow/constant_folding.hpp"
#include "logic/logical.hpp"
#include "logic/typing.hpp"

namespace michaelcc {
    namespace dataflow {
        transform_pass constant_folding_pass = transform_pass(
            std::make_unique<constant_folding::expression_pass>(), 
            std::make_unique<default_statement_pass>(), 
            [](const std::string& name) { return name; }
        );

        namespace constant_folding {
            // Helper to determine the "larger" int type for usual arithmetic conversions
            static typing::qual_type get_common_int_type(const typing::int_type* left, const typing::int_type* right) {
                // Compute size rank for each type
                auto size_rank = [](const typing::int_type* t) -> int {
                    switch (t->type_class()) {
                        case typing::CHAR_INT_CLASS:  return 0;
                        case typing::SHORT_INT_CLASS: return 1;
                        case typing::INT_INT_CLASS:   return t->is_long() ? 3 : 2;
                        case typing::LONG_INT_CLASS:  return 4;
                        default: return -1;
                    }
                };

                int left_rank = size_rank(left);
                int right_rank = size_rank(right);

                // Pick the larger type; if same rank, prefer unsigned
                const typing::int_type* result_type;
                if (left_rank > right_rank) {
                    result_type = left;
                } else if (right_rank > left_rank) {
                    result_type = right;
                } else {
                    // Same rank: prefer unsigned
                    result_type = left->is_unsigned() ? left : right;
                }

                return typing::qual_type(std::make_shared<typing::int_type>(
                    result_type->int_qualifiers(), 
                    result_type->type_class()
                ));
            }

            // Helper to create a boolean result type (int with value 0 or 1)
            static typing::qual_type get_bool_type() {
                return typing::qual_type(std::make_shared<typing::int_type>(
                    typing::NO_INT_QUALIFIER, 
                    typing::INT_INT_CLASS
                ));
            }

            // Helper to determine the "larger" float type for usual arithmetic conversions
            static typing::qual_type get_common_float_type(const typing::float_type* left, const typing::float_type* right) {
                // double has higher rank than float
                typing::float_class result_class = (left->type_class() == typing::DOUBLE_FLOAT_CLASS || 
                                                    right->type_class() == typing::DOUBLE_FLOAT_CLASS)
                    ? typing::DOUBLE_FLOAT_CLASS 
                    : typing::FLOAT_FLOAT_CLASS;
                return typing::qual_type(std::make_shared<typing::float_type>(result_class));
            }

            // Helper to fold float arithmetic operations
            static std::unique_ptr<logical_ir::expression> fold_float_arithmetic(
                token_type op, double left_val, double right_val, typing::qual_type result_type
            ) {
                double result_val;
                switch (op) {
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
                        result_val = left_val / right_val;
                        break;
                    case MICHAELCC_TOKEN_DOUBLE_AND:
                        return std::make_unique<logical_ir::integer_constant>(
                            (left_val != 0.0 && right_val != 0.0) ? 1 : 0, get_bool_type());
                    case MICHAELCC_TOKEN_DOUBLE_OR:
                        return std::make_unique<logical_ir::integer_constant>(
                            (left_val != 0.0 || right_val != 0.0) ? 1 : 0, get_bool_type());
                    case MICHAELCC_TOKEN_EQUALS:
                        return std::make_unique<logical_ir::integer_constant>(
                            (left_val == right_val) ? 1 : 0, get_bool_type());
                    case MICHAELCC_TOKEN_NOT_EQUALS:
                        return std::make_unique<logical_ir::integer_constant>(
                            (left_val != right_val) ? 1 : 0, get_bool_type());
                    case MICHAELCC_TOKEN_LESS:
                        return std::make_unique<logical_ir::integer_constant>(
                            (left_val < right_val) ? 1 : 0, get_bool_type());
                    case MICHAELCC_TOKEN_MORE:
                        return std::make_unique<logical_ir::integer_constant>(
                            (left_val > right_val) ? 1 : 0, get_bool_type());
                    case MICHAELCC_TOKEN_LESS_EQUAL:
                        return std::make_unique<logical_ir::integer_constant>(
                            (left_val <= right_val) ? 1 : 0, get_bool_type());
                    case MICHAELCC_TOKEN_MORE_EQUAL:
                        return std::make_unique<logical_ir::integer_constant>(
                            (left_val >= right_val) ? 1 : 0, get_bool_type());
                    default:
                        return nullptr;
                }
                return std::make_unique<logical_ir::floating_constant>(result_val, std::move(result_type));
            }

            std::unique_ptr<logical_ir::expression> expression_pass::dispatch(std::unique_ptr<logical_ir::arithmetic_operator>&& node) {
                auto* left_int = dynamic_cast<logical_ir::integer_constant*>(node->left().get());
                auto* right_int = dynamic_cast<logical_ir::integer_constant*>(node->right().get());
                auto* left_float = dynamic_cast<logical_ir::floating_constant*>(node->left().get());
                auto* right_float = dynamic_cast<logical_ir::floating_constant*>(node->right().get());

                // Integer + Integer
                if (left_int && right_int) {
                    typing::int_type* left_int_type = static_cast<typing::int_type*>(left_int->get_type().type().get());
                    typing::int_type* right_int_type = static_cast<typing::int_type*>(right_int->get_type().type().get());

                    uint64_t left_val = left_int->value();
                    uint64_t right_val = right_int->value();
                    uint64_t result_val = 0;
                    typing::qual_type result_type;

                    switch (node->get_operator()) {
                        case MICHAELCC_TOKEN_PLUS:
                            result_val = left_val + right_val;
                            result_type = get_common_int_type(left_int_type, right_int_type);
                            break;
                        case MICHAELCC_TOKEN_MINUS:
                            result_val = left_val - right_val;
                            result_type = get_common_int_type(left_int_type, right_int_type);
                            break;
                        case MICHAELCC_TOKEN_ASTERISK:
                            result_val = left_val * right_val;
                            result_type = get_common_int_type(left_int_type, right_int_type);
                            break;
                        case MICHAELCC_TOKEN_SLASH:
                            if (right_val == 0) return node;
                            if (left_int_type->is_unsigned() || right_int_type->is_unsigned()) {
                                result_val = left_val / right_val;
                            } else {
                                result_val = static_cast<uint64_t>(static_cast<int64_t>(left_val) / static_cast<int64_t>(right_val));
                            }
                            result_type = get_common_int_type(left_int_type, right_int_type);
                            break;
                        case MICHAELCC_TOKEN_MODULO:
                            if (right_val == 0) return node;
                            if (left_int_type->is_unsigned() || right_int_type->is_unsigned()) {
                                result_val = left_val % right_val;
                            } else {
                                result_val = static_cast<uint64_t>(static_cast<int64_t>(left_val) % static_cast<int64_t>(right_val));
                            }
                            result_type = get_common_int_type(left_int_type, right_int_type);
                            break;
                        case MICHAELCC_TOKEN_AND:
                            result_val = left_val & right_val;
                            result_type = get_common_int_type(left_int_type, right_int_type);
                            break;
                        case MICHAELCC_TOKEN_OR:
                            result_val = left_val | right_val;
                            result_type = get_common_int_type(left_int_type, right_int_type);
                            break;
                        case MICHAELCC_TOKEN_CARET:
                            result_val = left_val ^ right_val;
                            result_type = get_common_int_type(left_int_type, right_int_type);
                            break;
                        case MICHAELCC_TOKEN_BITSHIFT_LEFT:
                            result_val = left_val << right_val;
                            result_type = typing::qual_type(left_int->get_type());
                            break;
                        case MICHAELCC_TOKEN_BITSHIFT_RIGHT:
                            if (left_int_type->is_unsigned()) {
                                result_val = left_val >> right_val;
                            } else {
                                result_val = static_cast<uint64_t>(static_cast<int64_t>(left_val) >> right_val);
                            }
                            result_type = typing::qual_type(left_int->get_type());
                            break;
                        case MICHAELCC_TOKEN_DOUBLE_AND:
                            result_val = (left_val != 0 && right_val != 0) ? 1 : 0;
                            result_type = get_bool_type();
                            break;
                        case MICHAELCC_TOKEN_DOUBLE_OR:
                            result_val = (left_val != 0 || right_val != 0) ? 1 : 0;
                            result_type = get_bool_type();
                            break;
                        case MICHAELCC_TOKEN_EQUALS:
                            result_val = (left_val == right_val) ? 1 : 0;
                            result_type = get_bool_type();
                            break;
                        case MICHAELCC_TOKEN_NOT_EQUALS:
                            result_val = (left_val != right_val) ? 1 : 0;
                            result_type = get_bool_type();
                            break;
                        case MICHAELCC_TOKEN_LESS:
                            if (left_int_type->is_unsigned() && right_int_type->is_unsigned()) {
                                result_val = (left_val < right_val) ? 1 : 0;
                            } else {
                                result_val = (static_cast<int64_t>(left_val) < static_cast<int64_t>(right_val)) ? 1 : 0;
                            }
                            result_type = get_bool_type();
                            break;
                        case MICHAELCC_TOKEN_MORE:
                            if (left_int_type->is_unsigned() && right_int_type->is_unsigned()) {
                                result_val = (left_val > right_val) ? 1 : 0;
                            } else {
                                result_val = (static_cast<int64_t>(left_val) > static_cast<int64_t>(right_val)) ? 1 : 0;
                            }
                            result_type = get_bool_type();
                            break;
                        case MICHAELCC_TOKEN_LESS_EQUAL:
                            if (left_int_type->is_unsigned() && right_int_type->is_unsigned()) {
                                result_val = (left_val <= right_val) ? 1 : 0;
                            } else {
                                result_val = (static_cast<int64_t>(left_val) <= static_cast<int64_t>(right_val)) ? 1 : 0;
                            }
                            result_type = get_bool_type();
                            break;
                        case MICHAELCC_TOKEN_MORE_EQUAL:
                            if (left_int_type->is_unsigned() && right_int_type->is_unsigned()) {
                                result_val = (left_val >= right_val) ? 1 : 0;
                            } else {
                                result_val = (static_cast<int64_t>(left_val) >= static_cast<int64_t>(right_val)) ? 1 : 0;
                            }
                            result_type = get_bool_type();
                            break;
                        default:
                            return node;
                    }

                    return std::make_unique<logical_ir::integer_constant>(result_val, std::move(result_type));
                }

                // Float + Float
                if (left_float && right_float) {
                    typing::float_type* left_float_type = static_cast<typing::float_type*>(left_float->get_type().type().get());
                    typing::float_type* right_float_type = static_cast<typing::float_type*>(right_float->get_type().type().get());

                    auto result = fold_float_arithmetic(
                        node->get_operator(),
                        left_float->value(),
                        right_float->value(),
                        get_common_float_type(left_float_type, right_float_type)
                    );
                    return result ? std::move(result) : std::move(node);
                }

                // Float + Int (promote int to float)
                if (left_float && right_int) {
                    typing::float_type* float_type = static_cast<typing::float_type*>(left_float->get_type().type().get());
                    typing::int_type* int_type = static_cast<typing::int_type*>(right_int->get_type().type().get());

                    double right_val = int_type->is_unsigned() 
                        ? static_cast<double>(right_int->value())
                        : static_cast<double>(static_cast<int64_t>(right_int->value()));

                    auto result = fold_float_arithmetic(
                        node->get_operator(),
                        left_float->value(),
                        right_val,
                        typing::qual_type(std::make_shared<typing::float_type>(float_type->type_class()))
                    );
                    return result ? std::move(result) : std::move(node);
                }

                // Int + Float (promote int to float)
                if (left_int && right_float) {
                    typing::int_type* int_type = static_cast<typing::int_type*>(left_int->get_type().type().get());
                    typing::float_type* float_type = static_cast<typing::float_type*>(right_float->get_type().type().get());

                    double left_val = int_type->is_unsigned()
                        ? static_cast<double>(left_int->value())
                        : static_cast<double>(static_cast<int64_t>(left_int->value()));

                    auto result = fold_float_arithmetic(
                        node->get_operator(),
                        left_val,
                        right_float->value(),
                        typing::qual_type(std::make_shared<typing::float_type>(float_type->type_class()))
                    );
                    return result ? std::move(result) : std::move(node);
                }

                return node;
            }

            std::unique_ptr<logical_ir::expression> expression_pass::dispatch(std::unique_ptr<logical_ir::unary_operation>&& node) {
                const auto& operand = node->operand();

                logical_ir::integer_constant* integer_constant = dynamic_cast<logical_ir::integer_constant*>(operand.get());
                if (integer_constant) {
                    typing::int_type* int_type = static_cast<typing::int_type*>(integer_constant->get_type().type().get());

                    switch (node->get_operator()) {
                        case MICHAELCC_TOKEN_MINUS:
                            if (int_type->is_unsigned()) {
                                return node;
                            }

                            return std::make_unique<logical_ir::integer_constant>(
                                -integer_constant->value(), 
                                typing::qual_type(integer_constant->get_type())
                            );
                        case MICHAELCC_TOKEN_NOT:
                            return std::make_unique<logical_ir::integer_constant>(
                                integer_constant->value() == 0 ? 1 : 0, 
                                typing::qual_type(integer_constant->get_type())
                            );
                        case MICHAELCC_TOKEN_TILDE:
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
        }
    }
}
