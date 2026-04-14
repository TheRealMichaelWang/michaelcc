#include "linear/optimization/const_prop.hpp"
#include "linear/ir.hpp"
#include <memory>

void michaelcc::linear::optimization::const_prop_pass::prescan(const translation_unit& unit) {
    for (const auto& [block_id, block] : unit.blocks) {
        for (const auto& instruction : block.instructions()) {
            if (auto* init = dynamic_cast<linear::init_register*>(instruction.get())) {
                m_const_definitions.insert({ init->destination(), init->value() });
            }
            else if (auto* a2_instruction = dynamic_cast<linear::a2_instruction*>(instruction.get())) {
                m_a2_definitions.insert({ 
                    a2_instruction->destination(), linear::a2_instruction(
                        a2_instruction->type(), 
                        a2_instruction->destination(), 
                        a2_instruction->operand_a(), 
                        a2_instruction->constant()
                    ) 
                });
            }
        }
    }
}

bool michaelcc::linear::optimization::const_prop_pass::optimize(translation_unit& unit) {
    bool made_changes = false;

    instruction_pass pass(*this, unit);
    for (auto& [block_id, block] : unit.blocks) {
        m_current_block_id = block_id;
        auto released_instructions = block.release_instructions();

        std::vector<std::unique_ptr<instruction>> new_instructions;
        new_instructions.reserve(released_instructions.size());
        for (auto& instruction : released_instructions) {
            std::unique_ptr<linear::instruction> new_instruction = pass(*instruction.get());

            if (new_instruction) {
                made_changes = true;
                if (auto* old_init = dynamic_cast<linear::init_register*>(instruction.get())) {
                    m_const_definitions.erase(old_init->destination());
                }
                if (auto* new_init = dynamic_cast<linear::init_register*>(new_instruction.get())) {
                    m_const_definitions[new_init->destination()] = new_init->value();
                }
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
        if (node.operand_b().reg_class != MICHAELCC_REGISTER_CLASS_INTEGER) {
            return nullptr;
        } 
        //return std::make_unique<a2_instruction>(node.type(), node.destination(), node.operand_b(), constant);

        switch (node.type()) {
        case MICHAELCC_LINEAR_A_ADD:
        case MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY:
        case MICHAELCC_LINEAR_A_BITWISE_AND:
        case MICHAELCC_LINEAR_A_BITWISE_OR:
        case MICHAELCC_LINEAR_A_BITWISE_XOR:
        case MICHAELCC_LINEAR_A_BITWISE_NAND:
        case MICHAELCC_LINEAR_A_AND:
        case MICHAELCC_LINEAR_A_OR:
        case MICHAELCC_LINEAR_A_XOR:
        case MICHAELCC_LINEAR_A_COMPARE_EQUAL:
        case MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL:
        return std::make_unique<a2_instruction>(node.type(), node.destination(), node.operand_b(), constant);
        default:
            return nullptr;
        }
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

        if (node.operand_b().reg_class != MICHAELCC_REGISTER_CLASS_INTEGER) {
            return nullptr;
        }

        switch (node.type()) {
        case MICHAELCC_LINEAR_A_ADD:
        case MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY:
        case MICHAELCC_LINEAR_A_BITWISE_AND:
        case MICHAELCC_LINEAR_A_BITWISE_OR:
        case MICHAELCC_LINEAR_A_BITWISE_XOR:
        case MICHAELCC_LINEAR_A_BITWISE_NAND:
        case MICHAELCC_LINEAR_A_AND:
        case MICHAELCC_LINEAR_A_OR:
        case MICHAELCC_LINEAR_A_XOR:
        case MICHAELCC_LINEAR_A_COMPARE_EQUAL:
        case MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL:
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
    case MICHAELCC_LINEAR_A_SIGNED_MULTIPLY: return make_signed([](auto a, auto b) { return a * b; });
    case MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY: return make_int([](auto a, auto b) { return a * b; });
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
    case MICHAELCC_LINEAR_A_BITWISE_NAND:  return make_int([](auto a, auto b) { return ~(a & b); });

    case MICHAELCC_LINEAR_A_AND: return make_int_cmp([](auto a, auto b) { return a && b; });
    case MICHAELCC_LINEAR_A_OR:  return make_int_cmp([](auto a, auto b) { return a || b; });
    case MICHAELCC_LINEAR_A_XOR: return make_int_cmp([](auto a, auto b) { return (bool)a ^ (bool)b; });

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

    auto a2_definition = m_pass.m_a2_definitions.find(node.operand_a());
    if (a2_definition != m_pass.m_a2_definitions.end()) {
        auto& inner = a2_definition->second;
        auto a2_fold = [&](a_instruction_type result_type, auto compute) -> std::unique_ptr<instruction> {
            return std::make_unique<linear::a2_instruction>(
                result_type,
                node.destination(), 
                inner.operand_a(), 
                compute(inner.constant(), node.constant())
            );
        };

        if (node.type() == inner.type()) {
            switch (node.type()) {
            case MICHAELCC_LINEAR_A_ADD: return a2_fold(node.type(), [](auto a, auto b) { return a + b; });
            case MICHAELCC_LINEAR_A_SUBTRACT: return a2_fold(node.type(), [](auto a, auto b) { return a + b; });
            case MICHAELCC_LINEAR_A_SIGNED_MULTIPLY: return a2_fold(node.type(), [](auto a, auto b) { return a * b; });
            case MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY: return a2_fold(node.type(), [](auto a, auto b) { return a * b; });
            case MICHAELCC_LINEAR_A_BITWISE_AND: return a2_fold(node.type(), [](auto a, auto b) { return a & b; });
            case MICHAELCC_LINEAR_A_BITWISE_OR: return a2_fold(node.type(), [](auto a, auto b) { return a | b; });
            case MICHAELCC_LINEAR_A_BITWISE_XOR: return a2_fold(node.type(), [](auto a, auto b) { return a ^ b; });
            case MICHAELCC_LINEAR_A_BITWISE_NAND: return a2_fold(node.type(), [](auto a, auto b) { return ~(a & b); });
            default: break;
            };
        }

        // ADD then SUB (or vice versa): fold the constants against each other
        if (inner.type() == MICHAELCC_LINEAR_A_ADD && node.type() == MICHAELCC_LINEAR_A_SUBTRACT) {
            return a2_fold(MICHAELCC_LINEAR_A_ADD, [](auto a, auto b) { return a - b; });
        }
        if (inner.type() == MICHAELCC_LINEAR_A_SUBTRACT && node.type() == MICHAELCC_LINEAR_A_ADD) {
            return a2_fold(MICHAELCC_LINEAR_A_SUBTRACT, [](auto a, auto b) { return a - b; });
        }
    }
    
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
    case MICHAELCC_LINEAR_A_SIGNED_MULTIPLY: return fold_signed([](auto a, auto b) { return a * b; });
    case MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY: return fold_int([](auto a, auto b) { return a * b; });
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
    case MICHAELCC_LINEAR_A_BITWISE_NAND:    return fold_int([](auto a, auto b) { return ~(a & b); });
    case MICHAELCC_LINEAR_A_AND:             return fold_int([](auto a, auto b) { return a && b; });
    case MICHAELCC_LINEAR_A_OR:              return fold_int([](auto a, auto b) { return a || b; });
    case MICHAELCC_LINEAR_A_XOR:             return fold_int([](auto a, auto b) { return (bool)a ^ (bool)b; });
    case MICHAELCC_LINEAR_A_COMPARE_EQUAL:   return fold_int([](auto a, auto b) { return a == b; });
    case MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL: return fold_int([](auto a, auto b) { return a != b; });
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
    auto const_val = m_pass.get_const_value(node.operand());
    if (!const_val.has_value()) {
        return nullptr;
    }

    auto val = const_val.value();
    auto sz = node.operand().reg_size;
    auto dest = node.destination();

    switch (node.type()) {
    case MICHAELCC_LINEAR_U_NEGATE:
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:   return std::make_unique<init_register>(dest, register_word{ .sbyte = static_cast<int8_t>(-val.sbyte) });
        case MICHAELCC_WORD_SIZE_UINT16: return std::make_unique<init_register>(dest, register_word{ .int16 = static_cast<int16_t>(-val.int16) });
        case MICHAELCC_WORD_SIZE_UINT32: return std::make_unique<init_register>(dest, register_word{ .int32 = -val.int32 });
        case MICHAELCC_WORD_SIZE_UINT64: return std::make_unique<init_register>(dest, register_word{ .int64 = -val.int64 });
        default: return nullptr;
        }
    case MICHAELCC_LINEAR_U_FLOAT_NEGATE:
        switch (sz) {
        case MICHAELCC_WORD_SIZE_UINT32: return std::make_unique<init_register>(dest, register_word{ .float32 = -val.float32 });
        case MICHAELCC_WORD_SIZE_UINT64: return std::make_unique<init_register>(dest, register_word{ .float64 = -val.float64 });
        default: return nullptr;
        }
    case MICHAELCC_LINEAR_U_NOT:
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:   return std::make_unique<init_register>(dest, register_word{ .ubyte = static_cast<uint8_t>(val.ubyte ? 0 : 1) });
        case MICHAELCC_WORD_SIZE_UINT16: return std::make_unique<init_register>(dest, register_word{ .uint16 = static_cast<uint16_t>(val.uint16 ? 0 : 1) });
        case MICHAELCC_WORD_SIZE_UINT32: return std::make_unique<init_register>(dest, register_word{ .uint32 = val.uint32 ? 0u : 1u });
        case MICHAELCC_WORD_SIZE_UINT64: return std::make_unique<init_register>(dest, register_word{ .uint64 = val.uint64 ? 0ull : 1ull });
        default: return nullptr;
        }
    case MICHAELCC_LINEAR_U_BITWISE_NOT:
        switch (sz) {
        case MICHAELCC_WORD_SIZE_BYTE:   return std::make_unique<init_register>(dest, register_word{ .ubyte = static_cast<uint8_t>(~val.ubyte) });
        case MICHAELCC_WORD_SIZE_UINT16: return std::make_unique<init_register>(dest, register_word{ .uint16 = static_cast<uint16_t>(~val.uint16) });
        case MICHAELCC_WORD_SIZE_UINT32: return std::make_unique<init_register>(dest, register_word{ .uint32 = ~val.uint32 });
        case MICHAELCC_WORD_SIZE_UINT64: return std::make_unique<init_register>(dest, register_word{ .uint64 = ~val.uint64 });
        default: return nullptr;
        }
    default: return nullptr;
    }
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::load_memory& node) {
    auto a2_definition = m_pass.m_a2_definitions.find(node.source_address());
    if (a2_definition == m_pass.m_a2_definitions.end()) {
        return nullptr;
    }

    auto& inner = a2_definition->second;

    auto make_load_memory = [&](auto compute) -> std::unique_ptr<instruction> {
        return std::make_unique<load_memory>(node.destination(), inner.operand_a(), compute(node.offset(), inner.constant()));
    };

    switch(inner.type()) {
    case MICHAELCC_LINEAR_A_ADD: return make_load_memory([](auto a, auto b) { return a + b; });
    case MICHAELCC_LINEAR_A_SUBTRACT: return make_load_memory([](auto a, auto b) { return a - b; });
    default: return nullptr;
    }
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::store_memory& node) {
    auto a2_definition = m_pass.m_a2_definitions.find(node.destination_address());
    if (a2_definition == m_pass.m_a2_definitions.end()) {
        return nullptr;
    }
    
    auto& inner = a2_definition->second;
    auto make_store_memory = [&](auto compute) -> std::unique_ptr<instruction> {
        return std::make_unique<store_memory>(inner.operand_a(), node.value(), compute(node.offset(), inner.constant()));
    };
    
    switch(inner.type()) {
    case MICHAELCC_LINEAR_A_ADD: return make_store_memory([](auto a, auto b) { return a + b; });
    case MICHAELCC_LINEAR_A_SUBTRACT: return make_store_memory([](auto a, auto b) { return a - b; });
    default: return nullptr;
    }
}


std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::valloca_instruction& node) {
    auto length_const = m_pass.get_const_value(node.size());
    if (!length_const.has_value()) {
        return nullptr;
    }
    return std::make_unique<alloca_instruction>(node.destination(), length_const.value().uint64, node.alignment());
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::branch_condition& node) {
    auto condition_const = m_pass.get_const_value(node.condition());
    if (!condition_const.has_value()) {
        return nullptr;
    }

    if (condition_const.value().uint64 == 0) { //take false branch
        m_unit.blocks.at(m_pass.m_current_block_id.value()).remove_successor_block_id(node.if_true_block_id());
        m_unit.blocks.at(node.if_true_block_id()).remove_predecessor_block_id(m_pass.m_current_block_id.value());
        return std::make_unique<branch>(node.if_false_block_id());
    } else { //take true branch
        m_unit.blocks.at(m_pass.m_current_block_id.value()).remove_successor_block_id(node.if_false_block_id());
        m_unit.blocks.at(node.if_false_block_id()).remove_predecessor_block_id(m_pass.m_current_block_id.value());
        return std::make_unique<branch>(node.if_true_block_id());
    }
}

std::unique_ptr<michaelcc::linear::instruction> michaelcc::linear::optimization::const_prop_pass::instruction_pass::dispatch(const michaelcc::linear::phi_instruction& node) {
    std::optional<register_word> common_value;
    
    for (const auto& value : node.values()) {
        auto const_value = m_pass.get_const_value(value.vreg);
        if (!const_value.has_value()) {
            return nullptr;
        }
        if (!common_value.has_value()) {
            common_value = const_value;
        }
        else if (common_value.value().uint64 != const_value.value().uint64) {
            return nullptr;
        }
    }

    assert(common_value.has_value());
    return std::make_unique<init_register>(node.destination(), common_value.value());
}