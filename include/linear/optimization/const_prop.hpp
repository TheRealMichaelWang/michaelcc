#include "linear/ir.hpp"
#include "linear/optimization.hpp"

namespace michaelcc::linear::optimization {
    class const_prop_pass final : public pass {
    private:
        class instruction_pass : public instruction_transformer {
        private:
            const_prop_pass& m_pass;

        public:
            instruction_pass(const_prop_pass& pass) : m_pass(pass) {}

        protected:
            std::unique_ptr<instruction> dispatch(const a_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const a2_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const u_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const copy_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const valloca_instruction& node) override;
            std::unique_ptr<instruction> dispatch(const branch_condition& node) override;
            std::unique_ptr<instruction> dispatch(const phi_instruction& node) override;

            std::unique_ptr<instruction> handle_default(const instruction& node) override {
                return nullptr;
            }
        };

        std::unordered_map<virtual_register, init_register*> m_const_vregs;

    public:
        void prescan(const translation_unit& unit) override;

        bool optimize(translation_unit& unit) override;

        void reset() override { m_const_vregs.clear(); }
    };
}