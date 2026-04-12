#include "linear/optimization/gvn.hpp"
#include "linear/dominators.hpp"
#include "linear/ir.hpp"
#include "linear/registers.hpp"
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_map>

using michaelcc::linear::virtual_register;

void michaelcc::linear::optimization::gvn_pass::instruction_hasher::hash_combine(size_t& seed, size_t value) noexcept {
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

size_t michaelcc::linear::optimization::gvn_pass::instruction_hasher::hash_virtual_register(virtual_register v) noexcept {
    size_t h = 0;
    hash_combine(h, v.id);
    hash_combine(h, static_cast<size_t>(v.reg_size));
    hash_combine(h, static_cast<size_t>(v.reg_class));
    return h;
}

size_t michaelcc::linear::optimization::gvn_pass::instruction_hasher::hash_register_word(const register_word& w) noexcept {
    std::byte bytes[sizeof(register_word)];
    std::memcpy(bytes, &w, sizeof(register_word));
    size_t h = 0;
    for (size_t i = 0; i < sizeof(register_word); ++i) {
        hash_combine(h, static_cast<size_t>(bytes[i]));
    }
    return h;
}

size_t michaelcc::linear::optimization::gvn_pass::instruction_hasher::hash_function_parameter(const function_parameter& p) noexcept {
    size_t h = std::hash<std::string>{}(p.name);
    hash_combine(h, p.layout.size);
    hash_combine(h, p.layout.alignment);
    hash_combine(h, p.index);
    hash_combine(h, static_cast<size_t>(p.register_class));
    hash_combine(h, static_cast<size_t>(p.pass_via_stack));
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const a_instruction& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(1);
    hash_combine(h, static_cast<size_t>(node.type()));
    hash_combine(h, hash_virtual_register(node.operand_a()));
    hash_combine(h, hash_virtual_register(node.operand_b()));
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const a2_instruction& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(2);
    hash_combine(h, static_cast<size_t>(node.type()));
    hash_combine(h, hash_virtual_register(node.operand_a()));
    hash_combine(h, node.constant());
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const u_instruction& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(3);
    hash_combine(h, static_cast<size_t>(node.type()));
    hash_combine(h, hash_virtual_register(node.operand()));
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const c_instruction& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(4);
    hash_combine(h, static_cast<size_t>(node.type()));
    hash_combine(h, hash_virtual_register(node.source()));
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const init_register& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(5);
    hash_combine(h, hash_register_word(node.value()));
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const load_parameter& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(9);
    hash_combine(h, hash_function_parameter(node.parameter()));
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const load_effective_address& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(11);
    hash_combine(h, std::hash<std::string>{}(node.label()));
    return h;
}

bool michaelcc::linear::optimization::gvn_pass::instruction_comparator::dispatch(const a_instruction& node) {
    auto* other = dynamic_cast<const a_instruction*>(m_compare_to);
    if (!other) return false;
    return node.type() == other->type()
        && node.operand_a() == other->operand_a()
        && node.operand_b() == other->operand_b();
}

bool michaelcc::linear::optimization::gvn_pass::instruction_comparator::dispatch(const a2_instruction& node) {
    auto* other = dynamic_cast<const a2_instruction*>(m_compare_to);
    if (!other) return false;
    return node.type() == other->type()
        && node.operand_a() == other->operand_a()
        && node.constant() == other->constant();
}

bool michaelcc::linear::optimization::gvn_pass::instruction_comparator::dispatch(const u_instruction& node) {
    auto* other = dynamic_cast<const u_instruction*>(m_compare_to);
    if (!other) return false;
    return node.type() == other->type()
        && node.operand() == other->operand();
}

bool michaelcc::linear::optimization::gvn_pass::instruction_comparator::dispatch(const c_instruction& node) {
    auto* other = dynamic_cast<const c_instruction*>(m_compare_to);
    if (!other) return false;
    return node.type() == other->type()
        && node.source() == other->source();
}

bool michaelcc::linear::optimization::gvn_pass::instruction_comparator::dispatch(const init_register& node) {
    auto* other = dynamic_cast<const init_register*>(m_compare_to);
    if (!other) return false;

    switch (node.destination().reg_size) {
        case linear::word_size::MICHAELCC_WORD_SIZE_BYTE:
            return node.value().ubyte == other->value().ubyte;
        case linear::word_size::MICHAELCC_WORD_SIZE_UINT16:
            return node.value().uint16 == other->value().uint16;
        case linear::word_size::MICHAELCC_WORD_SIZE_UINT32:
            return node.value().uint32 == other->value().uint32;
        case linear::word_size::MICHAELCC_WORD_SIZE_UINT64:
            return node.value().uint64 == other->value().uint64;
        default:
            return false;
    }
    return false;
}

bool michaelcc::linear::optimization::gvn_pass::instruction_comparator::dispatch(const load_parameter& node) {
    auto* other = dynamic_cast<const load_parameter*>(m_compare_to);
    if (!other) return false;
    return node.parameter() == other->parameter();
}

bool michaelcc::linear::optimization::gvn_pass::instruction_comparator::dispatch(const load_effective_address& node) {
    auto* other = dynamic_cast<const load_effective_address*>(m_compare_to);
    if (!other) return false;
    return node.label() == other->label();
}

void michaelcc::linear::optimization::gvn_pass::prescan(const translation_unit& unit) {
    instruction_hasher hasher;
    for (const auto& [block_id, block] : unit.blocks) {
        for (const auto& instruction : block.instructions()) {
            if (!instruction->destination_register().has_value() || instruction->has_side_effects()) {
                continue;
            }

            auto hash = hasher(*instruction.get());
            if (!hash.has_value()) {
                continue;
            }

            m_instruction_cache.insert({ hash.value(), std::make_pair(instruction.get(), block_id) });
        }
    }
}

bool michaelcc::linear::optimization::gvn_pass::optimize(translation_unit& unit) {
    instruction_hasher hasher;
    bool made_changes = false;

    for (auto& [block_id, block] : unit.blocks) {
        auto released_instructions = block.release_instructions();
        std::vector<std::unique_ptr<instruction>> new_instructions;
        new_instructions.reserve(released_instructions.size());


        for (auto& instruction : released_instructions) {
            auto hash = hasher(*instruction.get());
            if (!hash.has_value() || !m_instruction_cache.contains(hash.value())) {
                new_instructions.emplace_back(std::move(instruction));
                continue;
            }

            auto [existing_instruction, existing_block_id] = m_instruction_cache.at(hash.value());

            if (existing_instruction == instruction.get()) {
                new_instructions.emplace_back(std::move(instruction));
                continue;
            }

            instruction_comparator comparator(existing_instruction);
            if (!comparator(*instruction.get()) || !linear::is_dominated_by(unit, existing_block_id, block_id)) {
                new_instructions.emplace_back(std::move(instruction));
                continue;
            }
            
            made_changes = true; 
            new_instructions.emplace_back(std::make_unique<c_instruction>(
                MICHAELCC_LINEAR_C_COPY_INIT, 
                instruction->destination_register().value(), 
                existing_instruction->destination_register().value())
            );
        }

        block.replace_instructions(std::move(new_instructions));
    }
    return made_changes;
}
