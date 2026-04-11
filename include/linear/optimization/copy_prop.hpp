#include "linear/optimization.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace michaelcc {
    namespace linear {
        namespace optimization {
            class copy_prop_pass final : public pass {
            private:
                class replace_dest_transform : public instruction_transformer {
                private:
                    virtual_register m_new_dest;

                public:
                    explicit replace_dest_transform(virtual_register new_dest) : m_new_dest(new_dest) {}

                protected:
                    std::unique_ptr<instruction> dispatch(const a_instruction& node) override {
                        return std::make_unique<a_instruction>(node.type(), m_new_dest, node.operand_a(), node.operand_b());
                    }
                    std::unique_ptr<instruction> dispatch(const a2_instruction& node) override {
                        return std::make_unique<a2_instruction>(node.type(), m_new_dest, node.operand_a(), node.constant());
                    }
                    std::unique_ptr<instruction> dispatch(const u_instruction& node) override {
                        return std::make_unique<u_instruction>(node.type(), m_new_dest, node.operand());
                    }
                    std::unique_ptr<instruction> dispatch(const c_instruction& node) override {
                        return std::make_unique<c_instruction>(node.type(), m_new_dest, node.source());
                    }
                    std::unique_ptr<instruction> dispatch(const init_register& node) override {
                        return std::make_unique<init_register>(m_new_dest, node.value());
                    }
                    std::unique_ptr<instruction> dispatch(const load_memory& node) override {
                        return std::make_unique<load_memory>(m_new_dest, node.source_address(), node.offset(), node.size_bytes());
                    }
                    std::unique_ptr<instruction> dispatch(const alloca_instruction& node) override {
                        return std::make_unique<alloca_instruction>(m_new_dest, node.size_bytes(), node.alignment());
                    }
                    std::unique_ptr<instruction> dispatch(const valloca_instruction& node) override {
                        return std::make_unique<valloca_instruction>(m_new_dest, node.size(), node.alignment());
                    }
                    std::unique_ptr<instruction> dispatch(const load_parameter& node) override {
                        return std::make_unique<load_parameter>(m_new_dest, node.parameter());
                    }
                    std::unique_ptr<instruction> dispatch(const function_call& node) override {
                        function_call::callable callee = node.callee();
                        std::vector<virtual_register> arguments = node.arguments();
                        return std::make_unique<function_call>(m_new_dest, std::move(callee), std::move(arguments));
                    }
                    std::unique_ptr<instruction> dispatch(const phi_instruction& node) override {
                        std::vector<var_info> values = node.values();
                        return std::make_unique<phi_instruction>(m_new_dest, std::move(values));
                    }
                    std::unique_ptr<instruction> dispatch(const load_effective_address& node) override {
                        return std::make_unique<load_effective_address>(m_new_dest, node.label());
                    }
                    std::unique_ptr<instruction> handle_default(const instruction&) override {
                        return nullptr;
                    }
                };

                std::unordered_map<instruction*, virtual_register> m_new_definitions;
                std::unordered_set<instruction*> m_dead_instructions;

            public:
                void prescan(const translation_unit& unit) override;
                bool optimize(translation_unit& unit) override;
                void reset() override { m_new_definitions.clear(); m_dead_instructions.clear(); }
            };
        }
    }
}
