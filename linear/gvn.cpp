#include "linear/optimization/gvn.hpp"
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

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

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const load_memory& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(6);
    hash_combine(h, hash_virtual_register(node.source_address()));
    hash_combine(h, static_cast<size_t>(static_cast<uint64_t>(node.offset())));
    hash_combine(h, node.size_bytes());
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const alloca_instruction& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(7);
    hash_combine(h, node.size_bytes());
    hash_combine(h, node.alignment());
    return h;
}

std::optional<size_t> michaelcc::linear::optimization::gvn_pass::instruction_hasher::dispatch(const valloca_instruction& node) {
    if (node.has_side_effects()) {
        return std::nullopt;
    }
    size_t h = tag(8);
    hash_combine(h, hash_virtual_register(node.size()));
    hash_combine(h, node.alignment());
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
