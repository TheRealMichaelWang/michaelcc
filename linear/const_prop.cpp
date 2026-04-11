#include "linear/optimization/const_prop.hpp"
#include <memory>

void michaelcc::linear::optimization::const_prop_pass::prescan(const translation_unit& unit) {
    for (const auto& [block_id, block] : unit.blocks) {
        for (const auto& instruction : block.instructions()) {
            if (auto init_register = dynamic_cast<linear::init_register*>(instruction.get())) {
                m_const_vregs[init_register->destination()] = init_register;
            }
        }
    }
}

bool michaelcc::linear::optimization::const_prop_pass::optimize(translation_unit& unit) {
    bool made_changes = false;

    instruction_pass pass(*this);
    for (auto& [block_id, block] : unit.blocks) {
        auto released_instructions = block.release_instructions();

        std::vector<std::unique_ptr<instruction>> new_instructions;
        new_instructions.reserve(released_instructions.size());
        for (auto& instruction : released_instructions) {
            std::unique_ptr<linear::instruction> new_instruction = pass(*instruction.get());

            if (new_instruction) {
                made_changes = true;
                new_instructions.emplace_back(std::move(new_instruction));
            } else {
                new_instructions.emplace_back(std::move(instruction));
            }
        }

        block.replace_instructions(std::move(new_instructions));
    }

    return made_changes;
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::a_instruction& node) {
    auto const_lhs = m_pass.get_const_value(node.operand_a());
    auto const_rhs = m_pass.get_const_value(node.operand_b());
    if (!const_lhs.has_value() || !const_rhs.has_value()) {
        return nullptr;
    }

    if (const_lhs.has_value() && !const_rhs.has_value()) {
        auto val = const_lhs.value();
        size_t constant;
        switch (node.operand_a().reg_size) {
        case MICHAELCC_WORD_SIZE_BYTE:   constant = val.ubyte; break;
        case MICHAELCC_WORD_SIZE_UINT16: constant = val.uint16; break;
        case MICHAELCC_WORD_SIZE_UINT32: constant = val.uint32; break;
        case MICHAELCC_WORD_SIZE_UINT64: constant = val.uint64; break;
        default: return nullptr;
        }
        if (node.operand_b().reg_class == MICHAELCC_REGISTER_CLASS_INTEGER) {
            return std::make_unique<a2_instruction>(node.type(), node.destination(), node.operand_b(), constant);
        } 
        return nullptr;
    }
    if (!const_lhs.has_value() && const_rhs.has_value()) {
        auto val = const_rhs.value();
        size_t constant;
        switch (node.operand_b().reg_size) {
        case MICHAELCC_WORD_SIZE_BYTE:   constant = val.ubyte; break;
        case MICHAELCC_WORD_SIZE_UINT16: constant = val.uint16; break;
        case MICHAELCC_WORD_SIZE_UINT32: constant = val.uint32; break;
        case MICHAELCC_WORD_SIZE_UINT64: constant = val.uint64; break;
        default: return nullptr;
        }

        switch (node.type()) {
        case MICHAELCC_LINEAR_A_ADD:
        case MICHAELCC_LINEAR_A_MULTIPLY:
        case MICHAELCC_LINEAR_A_BITWISE_AND:
        case MICHAELCC_LINEAR_A_BITWISE_OR:
        case MICHAELCC_LINEAR_A_BITWISE_XOR:
            return std::make_unique<a2_instruction>(node.type(), node.destination(), node.operand_b(), constant);
        default:
            return nullptr;
        }
    }

    auto lhs = const_lhs.value();
    auto rhs = const_rhs.value();
    auto sz = node.operand_a().reg_size;
    auto dest = node.destination();

    auto make_int = [&](auto compute) -> std::unique_ptr<instruction> {
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:
            return std::make_unique<init_register>(dest, register_word{ .ubyte = static_cast<uint8_t>(compute(lhs.ubyte, rhs.ubyte)) });
        case MICHAELCC_WORD_SIZE_UINT16:
            return std::make_unique<init_register>(dest, register_word{ .uint16 = static_cast<uint16_t>(compute(lhs.uint16, rhs.uint16)) });
        case MICHAELCC_WORD_SIZE_UINT32:
            return std::make_unique<init_register>(dest, register_word{ .uint32 = static_cast<uint32_t>(compute(lhs.uint32, rhs.uint32)) });
        case MICHAELCC_WORD_SIZE_UINT64:
            return std::make_unique<init_register>(dest, register_word{ .uint64 = static_cast<uint64_t>(compute(lhs.uint64, rhs.uint64)) });
        default: throw std::runtime_error("Invalid word size");
        }
    };

    auto make_signed = [&](auto compute) -> std::unique_ptr<instruction> {
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:
            return std::make_unique<init_register>(dest, register_word{ .sbyte = static_cast<int8_t>(compute(lhs.sbyte, rhs.sbyte)) });
        case MICHAELCC_WORD_SIZE_UINT16:
            return std::make_unique<init_register>(dest, register_word{ .int16 = static_cast<int16_t>(compute(lhs.int16, rhs.int16)) });
        case MICHAELCC_WORD_SIZE_UINT32:
            return std::make_unique<init_register>(dest, register_word{ .int32 = static_cast<int32_t>(compute(lhs.int32, rhs.int32)) });
        case MICHAELCC_WORD_SIZE_UINT64:
            return std::make_unique<init_register>(dest, register_word{ .int64 = static_cast<int64_t>(compute(lhs.int64, rhs.int64)) });
        default: throw std::runtime_error("Invalid word size");
        }
    };

    auto make_float = [&](auto compute) -> std::unique_ptr<instruction> {
        switch (sz) {
        case MICHAELCC_WORD_SIZE_UINT32:
            return std::make_unique<init_register>(dest, register_word{ .float32 = compute(lhs.float32, rhs.float32) });
        case MICHAELCC_WORD_SIZE_UINT64:
            return std::make_unique<init_register>(dest, register_word{ .float64 = compute(lhs.float64, rhs.float64) });
        default: throw std::runtime_error("Invalid word size for float");
        }
    };

    auto make_float_cmp = [&](auto compute) -> std::unique_ptr<instruction> {
        uint32_t result;
        switch (sz) {
        case MICHAELCC_WORD_SIZE_UINT32:
            result = compute(lhs.float32, rhs.float32) ? 1 : 0; break;
        case MICHAELCC_WORD_SIZE_UINT64:
            result = compute(lhs.float64, rhs.float64) ? 1 : 0; break;
        default: throw std::runtime_error("Invalid word size for float compare");
        }
        return std::make_unique<init_register>(dest, register_word{ .uint32 = result });
    };

    auto make_int_cmp = [&](auto compute) -> std::unique_ptr<instruction> {
        uint32_t result;
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:   result = compute(lhs.ubyte, rhs.ubyte) ? 1 : 0; break;
        case MICHAELCC_WORD_SIZE_UINT16: result = compute(lhs.uint16, rhs.uint16) ? 1 : 0; break;
        case MICHAELCC_WORD_SIZE_UINT32: result = compute(lhs.uint32, rhs.uint32) ? 1 : 0; break;
        case MICHAELCC_WORD_SIZE_UINT64: result = compute(lhs.uint64, rhs.uint64) ? 1 : 0; break;
        default: throw std::runtime_error("Invalid word size");
        }
        return std::make_unique<init_register>(dest, register_word{ .uint32 = result });
    };

    auto make_signed_cmp = [&](auto compute) -> std::unique_ptr<instruction> {
        uint32_t result;
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:   result = compute(lhs.sbyte, rhs.sbyte) ? 1 : 0; break;
        case MICHAELCC_WORD_SIZE_UINT16: result = compute(lhs.int16, rhs.int16) ? 1 : 0; break;
        case MICHAELCC_WORD_SIZE_UINT32: result = compute(lhs.int32, rhs.int32) ? 1 : 0; break;
        case MICHAELCC_WORD_SIZE_UINT64: result = compute(lhs.int64, rhs.int64) ? 1 : 0; break;
        default: throw std::runtime_error("Invalid word size");
        }
        return std::make_unique<init_register>(dest, register_word{ .uint32 = result });
    };

    switch (node.type()) {
    case MICHAELCC_LINEAR_A_ADD:           return make_int([](auto a, auto b) { return a + b; });
    case MICHAELCC_LINEAR_A_SUBTRACT:      return make_int([](auto a, auto b) { return a - b; });
    case MICHAELCC_LINEAR_A_MULTIPLY:      return make_int([](auto a, auto b) { return a * b; });
    case MICHAELCC_LINEAR_A_SIGNED_DIVIDE: return make_signed([](auto a, auto b) { return a / b; });
    case MICHAELCC_LINEAR_A_UNSIGNED_DIVIDE: return make_int([](auto a, auto b) { return a / b; });
    case MICHAELCC_LINEAR_A_SIGNED_MODULO: return make_signed([](auto a, auto b) { return a % b; });
    case MICHAELCC_LINEAR_A_UNSIGNED_MODULO: return make_int([](auto a, auto b) { return a % b; });

    case MICHAELCC_LINEAR_A_FLOAT_ADD:      return make_float([](auto a, auto b) { return a + b; });
    case MICHAELCC_LINEAR_A_FLOAT_SUBTRACT: return make_float([](auto a, auto b) { return a - b; });
    case MICHAELCC_LINEAR_A_FLOAT_MULTIPLY: return make_float([](auto a, auto b) { return a * b; });
    case MICHAELCC_LINEAR_A_FLOAT_DIVIDE:   return make_float([](auto a, auto b) { return a / b; });
    case MICHAELCC_LINEAR_A_FLOAT_MODULO:   return nullptr;

    case MICHAELCC_LINEAR_A_SHIFT_LEFT:          return make_int([](auto a, auto b) { return a << b; });
    case MICHAELCC_LINEAR_A_SIGNED_SHIFT_RIGHT:  return make_signed([](auto a, auto b) { return a >> b; });
    case MICHAELCC_LINEAR_A_UNSIGNED_SHIFT_RIGHT: return make_int([](auto a, auto b) { return a >> b; });
    case MICHAELCC_LINEAR_A_BITWISE_AND:   return make_int([](auto a, auto b) { return a & b; });
    case MICHAELCC_LINEAR_A_BITWISE_OR:    return make_int([](auto a, auto b) { return a | b; });
    case MICHAELCC_LINEAR_A_BITWISE_XOR:   return make_int([](auto a, auto b) { return a ^ b; });
    case MICHAELCC_LINEAR_A_BITWISE_NOT:   return nullptr;

    case MICHAELCC_LINEAR_A_AND: return make_int_cmp([](auto a, auto b) { return a && b; });
    case MICHAELCC_LINEAR_A_OR:  return make_int_cmp([](auto a, auto b) { return a || b; });
    case MICHAELCC_LINEAR_A_XOR: return make_int_cmp([](auto a, auto b) { return (bool)a ^ (bool)b; });
    case MICHAELCC_LINEAR_A_NOT: return nullptr;

    case MICHAELCC_LINEAR_A_COMPARE_EQUAL:     return make_int_cmp([](auto a, auto b) { return a == b; });
    case MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL: return make_int_cmp([](auto a, auto b) { return a != b; });

    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN:              return make_signed_cmp([](auto a, auto b) { return a < b; });
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN_OR_EQUAL:     return make_signed_cmp([](auto a, auto b) { return a <= b; });
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN:            return make_int_cmp([](auto a, auto b) { return a < b; });
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN_OR_EQUAL:   return make_int_cmp([](auto a, auto b) { return a <= b; });
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN:           return make_signed_cmp([](auto a, auto b) { return a > b; });
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN_OR_EQUAL:  return make_signed_cmp([](auto a, auto b) { return a >= b; });
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN:         return make_int_cmp([](auto a, auto b) { return a > b; });
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN_OR_EQUAL: return make_int_cmp([](auto a, auto b) { return a >= b; });

    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_EQUAL:              return make_float_cmp([](auto a, auto b) { return a == b; });
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_NOT_EQUAL:          return make_float_cmp([](auto a, auto b) { return a != b; });
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN:          return make_float_cmp([](auto a, auto b) { return a < b; });
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN_OR_EQUAL: return make_float_cmp([](auto a, auto b) { return a <= b; });
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN:       return make_float_cmp([](auto a, auto b) { return a > b; });
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN_OR_EQUAL: return make_float_cmp([](auto a, auto b) { return a >= b; });

    default: return nullptr;
    }
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::a2_instruction& node) {
    auto const_lhs = m_pass.get_const_value(node.operand_a());
    if (!const_lhs.has_value()) {
        return nullptr;
    }

    auto lhs = const_lhs.value();
    auto c = node.constant();
    auto sz = node.operand_a().reg_size;
    auto dest = node.destination();

    auto fold_int = [&](auto compute) -> std::unique_ptr<instruction> {
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:
            return std::make_unique<init_register>(dest, register_word{ .ubyte = static_cast<uint8_t>(compute(lhs.ubyte, static_cast<uint8_t>(c))) });
        case MICHAELCC_WORD_SIZE_UINT16:
            return std::make_unique<init_register>(dest, register_word{ .uint16 = static_cast<uint16_t>(compute(lhs.uint16, static_cast<uint16_t>(c))) });
        case MICHAELCC_WORD_SIZE_UINT32:
            return std::make_unique<init_register>(dest, register_word{ .uint32 = static_cast<uint32_t>(compute(lhs.uint32, static_cast<uint32_t>(c))) });
        case MICHAELCC_WORD_SIZE_UINT64:
            return std::make_unique<init_register>(dest, register_word{ .uint64 = static_cast<uint64_t>(compute(lhs.uint64, static_cast<uint64_t>(c))) });
        default: return nullptr;
        }
    };

    auto fold_signed = [&](auto compute) -> std::unique_ptr<instruction> {
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:
            return std::make_unique<init_register>(dest, register_word{ .sbyte = static_cast<int8_t>(compute(lhs.sbyte, static_cast<int8_t>(c))) });
        case MICHAELCC_WORD_SIZE_UINT16:
            return std::make_unique<init_register>(dest, register_word{ .int16 = static_cast<int16_t>(compute(lhs.int16, static_cast<int16_t>(c))) });
        case MICHAELCC_WORD_SIZE_UINT32:
            return std::make_unique<init_register>(dest, register_word{ .int32 = static_cast<int32_t>(compute(lhs.int32, static_cast<int32_t>(c))) });
        case MICHAELCC_WORD_SIZE_UINT64:
            return std::make_unique<init_register>(dest, register_word{ .int64 = static_cast<int64_t>(compute(lhs.int64, static_cast<int64_t>(c))) });
        default: return nullptr;
        }
    };

    // unofficially this is the list of valid a instructions for a2_instruction
    switch (node.type()) {
    case MICHAELCC_LINEAR_A_ADD:             return fold_int([](auto a, auto b) { return a + b; });
    case MICHAELCC_LINEAR_A_SUBTRACT:        return fold_int([](auto a, auto b) { return a - b; });
    case MICHAELCC_LINEAR_A_MULTIPLY:        return fold_int([](auto a, auto b) { return a * b; });
    case MICHAELCC_LINEAR_A_SIGNED_DIVIDE:   return fold_signed([](auto a, auto b) { return a / b; });
    case MICHAELCC_LINEAR_A_UNSIGNED_DIVIDE: return fold_int([](auto a, auto b) { return a / b; });
    case MICHAELCC_LINEAR_A_SIGNED_MODULO:   return fold_signed([](auto a, auto b) { return a % b; });
    case MICHAELCC_LINEAR_A_UNSIGNED_MODULO: return fold_int([](auto a, auto b) { return a % b; });
    case MICHAELCC_LINEAR_A_SHIFT_LEFT:           return fold_int([](auto a, auto b) { return a << b; });
    case MICHAELCC_LINEAR_A_SIGNED_SHIFT_RIGHT:   return fold_signed([](auto a, auto b) { return a >> b; });
    case MICHAELCC_LINEAR_A_UNSIGNED_SHIFT_RIGHT: return fold_int([](auto a, auto b) { return a >> b; });
    case MICHAELCC_LINEAR_A_BITWISE_AND:     return fold_int([](auto a, auto b) { return a & b; });
    case MICHAELCC_LINEAR_A_BITWISE_OR:      return fold_int([](auto a, auto b) { return a | b; });
    case MICHAELCC_LINEAR_A_BITWISE_XOR:     return fold_int([](auto a, auto b) { return a ^ b; });
    default: return nullptr;
    }
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::c_instruction& node) {
    auto const_value = m_pass.get_const_value(node.source());
    if (!const_value.has_value()) {
        return nullptr;
    }

    auto val = const_value.value();
    auto dest = node.destination();

    switch (node.type()) {
    case MICHAELCC_LINEAR_C_FLOAT64_TO_SIGNED_INT64:
        return std::make_unique<init_register>(dest, register_word{ .int64 = static_cast<int64_t>(val.float64) });
    case MICHAELCC_LINEAR_C_FLOAT64_TO_UNSIGNED_INT64:
        return std::make_unique<init_register>(dest, register_word{ .uint64 = static_cast<uint64_t>(val.float64) });
    case MICHAELCC_LINEAR_C_FLOAT32_TO_SIGNED_INT32:
        return std::make_unique<init_register>(dest, register_word{ .int32 = static_cast<int32_t>(val.float32) });
    case MICHAELCC_LINEAR_C_FLOAT32_TO_UNSIGNED_INT32:
        return std::make_unique<init_register>(dest, register_word{ .uint32 = static_cast<uint32_t>(val.float32) });

    case MICHAELCC_LINEAR_C_SIGNED_INT64_TO_FLOAT64:
        return std::make_unique<init_register>(dest, register_word{ .float64 = static_cast<double>(val.int64) });
    case MICHAELCC_LINEAR_C_UNSIGNED_INT64_TO_FLOAT64:
        return std::make_unique<init_register>(dest, register_word{ .float64 = static_cast<double>(val.uint64) });
    case MICHAELCC_LINEAR_C_SIGNED_INT32_TO_FLOAT32:
        return std::make_unique<init_register>(dest, register_word{ .float32 = static_cast<float>(val.int32) });
    case MICHAELCC_LINEAR_C_UNSIGNED_INT32_TO_FLOAT32:
        return std::make_unique<init_register>(dest, register_word{ .float32 = static_cast<float>(val.uint32) });

    case MICHAELCC_LINEAR_C_FLOAT32_TO_FLOAT64:
        return std::make_unique<init_register>(dest, register_word{ .float64 = static_cast<double>(val.float32) });
    case MICHAELCC_LINEAR_C_FLOAT64_TO_FLOAT32:
        return std::make_unique<init_register>(dest, register_word{ .float32 = static_cast<float>(val.float64) });

    case MICHAELCC_LINEAR_C_SEXT_OR_TRUNC: {
        int64_t signed_val;
        switch (node.source().reg_size) {
        case MICHAELCC_WORD_SIZE_BYTE:   signed_val = val.sbyte; break;
        case MICHAELCC_WORD_SIZE_UINT16: signed_val = val.int16; break;
        case MICHAELCC_WORD_SIZE_UINT32: signed_val = val.int32; break;
        case MICHAELCC_WORD_SIZE_UINT64: signed_val = val.int64; break;
        default: return nullptr;
        }
        switch (dest.reg_size) {
        case MICHAELCC_WORD_SIZE_BYTE:   return std::make_unique<init_register>(dest, register_word{ .sbyte = static_cast<int8_t>(signed_val) });
        case MICHAELCC_WORD_SIZE_UINT16: return std::make_unique<init_register>(dest, register_word{ .int16 = static_cast<int16_t>(signed_val) });
        case MICHAELCC_WORD_SIZE_UINT32: return std::make_unique<init_register>(dest, register_word{ .int32 = static_cast<int32_t>(signed_val) });
        case MICHAELCC_WORD_SIZE_UINT64: return std::make_unique<init_register>(dest, register_word{ .int64 = signed_val });
        default: return nullptr;
        }
    }
    case MICHAELCC_LINEAR_C_ZEXT_OR_TRUNC: {
        uint64_t unsigned_val;
        switch (node.source().reg_size) {
        case MICHAELCC_WORD_SIZE_BYTE:   unsigned_val = val.ubyte; break;
        case MICHAELCC_WORD_SIZE_UINT16: unsigned_val = val.uint16; break;
        case MICHAELCC_WORD_SIZE_UINT32: unsigned_val = val.uint32; break;
        case MICHAELCC_WORD_SIZE_UINT64: unsigned_val = val.uint64; break;
        default: return nullptr;
        }
        switch (dest.reg_size) {
        case MICHAELCC_WORD_SIZE_BYTE:   return std::make_unique<init_register>(dest, register_word{ .ubyte = static_cast<uint8_t>(unsigned_val) });
        case MICHAELCC_WORD_SIZE_UINT16: return std::make_unique<init_register>(dest, register_word{ .uint16 = static_cast<uint16_t>(unsigned_val) });
        case MICHAELCC_WORD_SIZE_UINT32: return std::make_unique<init_register>(dest, register_word{ .uint32 = static_cast<uint32_t>(unsigned_val) });
        case MICHAELCC_WORD_SIZE_UINT64: return std::make_unique<init_register>(dest, register_word{ .uint64 = unsigned_val });
        default: return nullptr;
        }
    }
    case MICHAELCC_LINEAR_C_COPY_INIT:
        return std::make_unique<init_register>(dest, val);

    default: return nullptr;
    }
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::u_instruction& node) {

}