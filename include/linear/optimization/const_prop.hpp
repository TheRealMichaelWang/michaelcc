#include "linear/ir.hpp"
#include "linear/pass.hpp"
#include "linear/registers.hpp"
#include <optional>

namespace michaelcc::linear::optimization {
    class const_prop_pass final : public pass {
    private:
        class instruction_pass : public instruction_transformer {
        private:
            const_prop_pass& m_pass;
            translation_unit& m_unit;

        public:
            instruction_pass(const_prop_pass& pass, translation_unit& unit) : m_pass(pass), m_unit(unit) {}

        protected:
            std::unique_ptr<instruction> dispatch(const a_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const a2_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const u_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const c_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const valloca_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const branch_condition& node) override;
            std::unique_ptr<instruction> dispatch(const phi_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const load_memory& node) override;
            std::unique_ptr<instruction> dispatch(const store_memory& node) override;

            std::unique_ptr<instruction> handle_default(const instruction& node) override {
                return nullptr;
            }
        };

        std::unordered_map<virtual_register, register_word> m_const_definitions;
        std::unordered_map<virtual_register, a2_instruction> m_a2_definitions;
        std::optional<size_t> m_current_block_id = std::nullopt;

        std::optional<register_word> get_const_value(virtual_register vreg) const {
            auto it = m_const_definitions.find(vreg);
            if (it != m_const_definitions.end()) {
                return it->second;
            }
            return std::nullopt;
        }
    public:
        void prescan(const translation_unit& unit) override;

        bool optimize(translation_unit& unit) override;

        void reset() override { m_const_definitions.clear(); m_current_block_id = std::nullopt; }
    };
}