#include "linear/pass.hpp"
#include "linear/registers.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <variant>

namespace michaelcc {
    namespace linear {
        namespace optimization {

            class replace_operands_transform : public instruction_transformer {
            private:
                std::unordered_map<virtual_register, virtual_register> m_vreg_substitutions;

                virtual_register get_replacement(virtual_register vreg) const {
                    auto it = m_vreg_substitutions.find(vreg);
                    if (it != m_vreg_substitutions.end()) {
                        return it->second;
                    }
                    return vreg;
                }

            public:
                explicit replace_operands_transform(std::unordered_map<virtual_register, virtual_register> vreg_substitutions) 
                    : m_vreg_substitutions(std::move(vreg_substitutions)) {}

            protected:
                std::unique_ptr<instruction> dispatch(const a_instruction& node) override {
                    auto a = get_replacement(node.operand_a());
                    auto b = get_replacement(node.operand_b());
                    if (a != node.operand_a() || b != node.operand_b()) {
                        return std::make_unique<a_instruction>(node.type(), node.destination(), a, b);
                    }
                    return nullptr;
                }
                std::unique_ptr<instruction> dispatch(const a2_instruction& node) override {
                    auto a = get_replacement(node.operand_a());
                    if (a != node.operand_a()) {
                        return std::make_unique<a2_instruction>(node.type(), node.destination(), a, node.constant());
                    }
                    return nullptr;
                }
                std::unique_ptr<instruction> dispatch(const u_instruction& node) override {
                    auto a = get_replacement(node.operand());
                    if (a != node.operand()) {
                        return std::make_unique<u_instruction>(node.type(), node.destination(), a);
                    }
                    return nullptr;
                }
                std::unique_ptr<instruction> dispatch(const c_instruction& node) override {
                    auto a = get_replacement(node.source());
                    if (a != node.source()) {
                        return std::make_unique<c_instruction>(node.type(), node.destination(), a);
                    }
                    return nullptr;
                }

                std::unique_ptr<instruction> dispatch(const load_memory& node) override {
                    auto a = get_replacement(node.source_address());
                    if (a != node.source_address()) {
                        return std::make_unique<load_memory>(node.destination(), a, node.offset());
                    }
                    return nullptr;
                }

                std::unique_ptr<instruction> dispatch(const valloca_instruction& node) override {
                    auto a = get_replacement(node.size());
                    if (a != node.size()) {
                        return std::make_unique<valloca_instruction>(node.destination(), a, node.alignment());
                    }
                    return nullptr;
                }

                std::unique_ptr<instruction> dispatch(const phi_instruction& node) override {
                    std::vector<var_info> values = node.values();
                    bool any_changed = false;
                    for (auto& value : values) {
                        auto replacement = get_replacement(value.vreg);
                        if (replacement != value.vreg) {
                            value.vreg = replacement;
                            any_changed = true;
                        }
                    }
                    if (any_changed) {
                        return std::make_unique<phi_instruction>(node.destination(), std::move(values));
                    }
                    return nullptr;
                }

                std::unique_ptr<instruction> handle_default(const instruction&) override {
                    return nullptr;
                }

                std::unique_ptr<instruction> dispatch(const push_function_argument& node) override {
                    auto a = get_replacement(node.value());
                    if (a != node.value()) {
                        return std::make_unique<push_function_argument>(node.argument(), a, node.function_call_id());
                    }
                    return nullptr;
                }

                std::unique_ptr<instruction> dispatch(const function_call& node) override {
                    auto* callee_vreg = std::get_if<virtual_register>(&node.callee());
                    if (callee_vreg) {
                        auto a = get_replacement(*callee_vreg);
                        if (a != *callee_vreg) {
                            auto nc = std::make_unique<function_call>(
                                node.destination(),
                                function_call::callable(a),
                                node.argument_count(),
                                node.function_call_id()
                            );
                            nc->set_caller_saved_registers(
                                std::vector<virtual_register>(node.caller_saved_registers()));
                            return nc;
                        }
                    }
                    return nullptr;
                }
                
                std::unique_ptr<instruction> dispatch(const branch_condition& node) override {
                    auto a = get_replacement(node.condition());
                    if (a != node.condition()) {
                        return std::make_unique<branch_condition>(a, node.if_true_block_id(), node.if_false_block_id(), node.is_loop());
                    }
                    return nullptr;
                }

                std::unique_ptr<instruction> dispatch(const store_memory& node) override {
                    auto a = get_replacement(node.destination_address());
                    auto v = get_replacement(node.value());
                    if (a != node.destination_address() || v != node.value()) {
                        return std::make_unique<store_memory>(a, v, node.offset());
                    }
                    return nullptr;
                }
            };

            class copy_prop_pass final : public pass {
            private:
                std::unordered_map<virtual_register, size_t> m_instruction_map;

            public:
                void prescan(const translation_unit& unit) override;
                bool optimize(translation_unit& unit) override;
                void reset() override { m_instruction_map.clear(); }
            };
        }
    }
}
