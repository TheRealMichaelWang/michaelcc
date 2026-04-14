#ifndef MICHAELCC_ISA_X64_HPP
#define MICHAELCC_ISA_X64_HPP

#include "platform.hpp"
#include "assembly/assembler.hpp"
#include "isa/isa.hpp"
#include <vector>

namespace michaelcc::isa::x64 {
    class x64_assembler : public michaelcc::assembly::assembler {
    private:
        struct block_preamble_info {
            std::vector<linear::virtual_register> set_to_true;
            std::vector<linear::virtual_register> set_to_false;
        };

        std::unordered_map<size_t, block_preamble_info> m_block_preamble_info;

        struct pending_call_info {
            size_t num_stack_arg_bytes = 0;
            std::vector<linear::register_info> saved_int_regs;
            std::vector<linear::register_info> saved_float_regs;
        };
        std::unordered_map<size_t, pending_call_info> m_pending_calls;

        std::vector<linear::register_t> m_callee_saved_regs;

        void add_set_true_to_block_preamble(size_t block_id, linear::virtual_register vreg) {
            auto it = m_block_preamble_info.find(block_id);
            if (it != m_block_preamble_info.end()) {
                it->second.set_to_true.push_back(vreg);
            } else {
                m_block_preamble_info.insert({ block_id, block_preamble_info{ { vreg }, {} } });
            }
        }

        void add_set_false_to_block_preamble(size_t block_id, linear::virtual_register vreg) {
            auto it = m_block_preamble_info.find(block_id);
            if (it != m_block_preamble_info.end()) {
                it->second.set_to_false.push_back(vreg);
            } else {
                m_block_preamble_info.insert({ block_id, block_preamble_info{ {}, { vreg } } });
            }
        }
    public:
        x64_assembler(std::ostream& output) : michaelcc::assembly::assembler(output) {}

    protected:
        void begin_block_preamble(const linear::basic_block& block) override;
        void begin_function_preamble(const linear::function_definition& definition) override;
        void begin_function_call(const linear::function_call& instruction) override;

        void emit_unsigned_multiply(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);
        void emit_signed_multiply(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);
        void emit_signed_divide(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);
        void emit_signed_remainder(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);
        void emit_unsigned_divide(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);
        void emit_unsigned_remainder(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);

        void emit_logical_and(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);
        void emit_logical_or(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);
        void emit_logical_xor(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b);

        void dispatch(const linear::a_instruction& instruction) override;
        void dispatch(const linear::a2_instruction& instruction) override;
        void dispatch(const linear::u_instruction& instruction) override;
        void dispatch(const linear::c_instruction& instruction) override;
        void dispatch(const linear::init_register& instruction) override;
        void dispatch(const linear::load_parameter& instruction) override;
        void dispatch(const linear::load_effective_address& instruction) override;
        void dispatch(const linear::load_memory& instruction) override;
        void dispatch(const linear::store_memory& instruction) override;
        void dispatch(const linear::alloca_instruction& instruction) override;
        void dispatch(const linear::valloca_instruction& instruction) override;
        void dispatch(const linear::branch& instruction) override;
        void dispatch(const linear::branch_condition& instruction) override;
        void dispatch(const linear::push_function_argument& instruction) override;
        void dispatch(const linear::function_call& instruction) override;
        void dispatch(const linear::function_return& instruction) override;
    };

    class x64_isa : public isa {
    public:
        const platform_info& get_platform_info() const noexcept override;
        std::unique_ptr<assembly::assembler> create_assembler(std::ostream& output) const override { return std::make_unique<x64_assembler>(output); }
        void legalize(linear::translation_unit& unit) const override { }
    };
}

#endif