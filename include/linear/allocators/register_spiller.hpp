#ifndef MICHAELCC_LINEAR_ALLOCATORS_REGISTER_SPILLER_HPP
#define MICHAELCC_LINEAR_ALLOCATORS_REGISTER_SPILLER_HPP

#include "linear/ir.hpp"
#include "linear/registers.hpp"
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace michaelcc::linear::allocators {
    class register_spiller {
    private:
        translation_unit& m_translation_unit;
        std::unordered_set<michaelcc::linear::virtual_register> m_spilled_vregs;    

        std::unordered_map<michaelcc::linear::virtual_register, std::vector<michaelcc::linear::virtual_register>> m_spill_address;

        virtual_register get_value(virtual_register vreg, std::vector<std::unique_ptr<instruction>>& new_instructions) const;

        void spill_block(size_t block_id);

        class replace_operands_transform : public instruction_transformer {
        private:
            register_spiller& m_spiller;
            std::vector<std::unique_ptr<instruction>>& m_new_instructions;

        public:
            replace_operands_transform(register_spiller& spiller, std::vector<std::unique_ptr<instruction>>& new_instructions) 
            : m_spiller(spiller), m_new_instructions(new_instructions) {}

        protected:
            std::unique_ptr<instruction> dispatch(const a_instruction& node) override {
                return std::make_unique<a_instruction>(
                    node.type(),
                    node.destination(),
                    m_spiller.get_value(node.operand_a(), m_new_instructions),
                    m_spiller.get_value(node.operand_b(), m_new_instructions));
            }

            std::unique_ptr<instruction> dispatch(const a2_instruction& node) override {
                return std::make_unique<a2_instruction>(
                    node.type(),
                    node.destination(),
                    m_spiller.get_value(node.operand_a(), m_new_instructions),
                    node.constant());
            }

            std::unique_ptr<instruction> dispatch(const u_instruction& node) override {
                return std::make_unique<u_instruction>(
                    node.type(),
                    node.destination(),
                    m_spiller.get_value(node.operand(), m_new_instructions));
            }

            std::unique_ptr<instruction> dispatch(const c_instruction& node) override {
                return std::make_unique<c_instruction>(
                    node.type(),
                    node.destination(),
                    m_spiller.get_value(node.source(), m_new_instructions));
            }

            std::unique_ptr<instruction> dispatch(const load_memory& node) override {
                return std::make_unique<load_memory>(
                    node.destination(),
                    m_spiller.get_value(node.source_address(), m_new_instructions),
                    node.offset());
            }

            std::unique_ptr<instruction> dispatch(const valloca_instruction& node) override {
                return std::make_unique<valloca_instruction>(
                    node.destination(),
                    m_spiller.get_value(node.size(), m_new_instructions),
                    node.alignment());
            }

            std::unique_ptr<instruction> dispatch(const push_function_argument& node) override {
                return std::make_unique<push_function_argument>(
                    node.argument(),
                    m_spiller.get_value(node.value(), m_new_instructions));
            }

            std::unique_ptr<instruction> dispatch(const function_call& node) override {
                return std::make_unique<function_call>(
                    node.destination(),
                    std::visit(overloaded{
                        [this](const std::string& label) -> function_call::callable { return label; },
                        [this](const virtual_register& vreg) -> function_call::callable { return m_spiller.get_value(vreg, m_new_instructions); }
                    }, node.callee()),
                    node.argument_count());
            }

            std::unique_ptr<instruction> dispatch(const store_memory& node) override {
                return std::make_unique<store_memory>(
                    node.value(),
                    m_spiller.get_value(node.source_address(), m_new_instructions),
                    node.offset());
            }

            std::unique_ptr<instruction> dispatch(const branch_condition& node) override {
                return std::make_unique<branch_condition>(
                    m_spiller.get_value(node.condition(), m_new_instructions),
                    node.if_true_block_id(),
                    node.if_false_block_id(),
                    node.is_loop());
            }

            std::unique_ptr<instruction> handle_default(const instruction& node) override {
                return nullptr;
            }
        };

    public:
        register_spiller(translation_unit& translation_unit, const std::vector<virtual_register>& spilled_vregs) 
            : m_translation_unit(translation_unit), m_spilled_vregs(spilled_vregs.begin(), spilled_vregs.end()) {}

        void spill();
    };
}

#endif