#ifndef MICHAELCC_ASSEMBLY_ASSEMBLER_HPP
#define MICHAELCC_ASSEMBLY_ASSEMBLER_HPP

#include "linear/ir.hpp"
#include <stdexcept>

namespace michaelcc::assembly {
    class assembler: public linear::instruction_dispatcher<void> {
    private:
        std::ostream& m_output;
    
    public:

        assembler(std::ostream& output) : m_output(output) {}

        virtual ~assembler() = default;

    private:
        void assemble_block(const linear::translation_unit& unit, size_t block_id);

    protected:
        // use this to potentially save caller-saved registers for a function call
        virtual void begin_function_call(const linear::function_call& instruction, std::ostream& output) = 0;

        void dispatch(const linear::a_instruction& instruction) override = 0;
        void dispatch(const linear::a2_instruction& instruction) override = 0;
        void dispatch(const linear::u_instruction& instruction) override = 0;
        void dispatch(const linear::c_instruction& instruction) override = 0;
        void dispatch(const linear::init_register& instruction) override = 0;
        void dispatch(const linear::load_parameter& instruction) override = 0;
        void dispatch(const linear::load_effective_address& instruction) override = 0;
        void dispatch(const linear::load_memory& instruction) override = 0;
        void dispatch(const linear::store_memory& instruction) override = 0;
        void dispatch(const linear::alloca_instruction& instruction) override = 0;
        void dispatch(const linear::valloca_instruction& instruction) override = 0;
        void dispatch(const linear::branch& instruction) override = 0;
        void dispatch(const linear::branch_condition& instruction) override = 0;
        void dispatch(const linear::push_function_argument& instruction) override = 0;
        void dispatch(const linear::function_call& instruction) override = 0;
        void dispatch(const linear::function_return& instruction) override = 0;
        void dispatch(const linear::phi_instruction& instruction) override { throw std::runtime_error("phi instructions are not supported by the assembler"); }

    public:
        void assemble(const linear::translation_unit& unit);
    };
}

#endif