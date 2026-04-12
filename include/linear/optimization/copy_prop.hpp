#include "linear/optimization.hpp"
#include "linear/registers.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace michaelcc {
    namespace linear {
        namespace optimization {
            class copy_prop_pass final : public pass {
            private:
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
                        return std::make_unique<a_instruction>(
                            node.type(), 
                            node.destination(), 
                            get_replacement(node.operand_a()), 
                            get_replacement(node.operand_b())
                        );
                    }
                    std::unique_ptr<instruction> dispatch(const a2_instruction& node) override {
                        return std::make_unique<a2_instruction>(
                            node.type(), 
                            node.destination(), 
                            get_replacement(node.operand_a()), 
                            node.constant()
                        );
                    }
                    std::unique_ptr<instruction> dispatch(const u_instruction& node) override {
                        return std::make_unique<u_instruction>(
                            node.type(), 
                            node.destination(), 
                            get_replacement(node.operand())
                        );
                    }
                    std::unique_ptr<instruction> dispatch(const c_instruction& node) override {
                        return std::make_unique<c_instruction>(
                            node.type(), 
                            node.destination(), 
                            get_replacement(node.source())
                        );
                    }

                    std::unique_ptr<instruction> dispatch(const load_memory& node) override {
                        return std::make_unique<load_memory>(
                            node.destination(), 
                            get_replacement(node.source_address()), 
                            node.offset(), 
                            node.size_bytes()
                        );
                    }

                    std::unique_ptr<instruction> dispatch(const valloca_instruction& node) override {
                        return std::make_unique<valloca_instruction>(
                            node.destination(), 
                            get_replacement(node.size()), 
                            node.alignment()
                        );
                    }

                    std::unique_ptr<instruction> dispatch(const phi_instruction& node) override {
                        std::vector<var_info> values = node.values();
                        for (auto& value : values) {
                            value.vreg = get_replacement(value.vreg);
                        }
                        return std::make_unique<phi_instruction>(node.destination(), std::move(values));
                    }

                    std::unique_ptr<instruction> handle_default(const instruction&) override {
                        return nullptr;
                    }
                };
                std::unordered_map<virtual_register, std::pair<instruction*, size_t>> m_instruction_map;

            public:
                void prescan(const translation_unit& unit) override;
                bool optimize(translation_unit& unit) override;
                void reset() override { m_instruction_map.clear(); }
            };
        }
    }
}
