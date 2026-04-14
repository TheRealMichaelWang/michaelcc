#ifndef MICHAELCC_ASSEMBLY_ASSEMBLER_HPP
#define MICHAELCC_ASSEMBLY_ASSEMBLER_HPP

#include "linear/ir.hpp"

namespace michaelcc::assembly {
    class assembler {
    public:
        virtual ~assembler() = default;

        virtual void assemble_arithmetic(const linear::a_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_a2(const linear::a2_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_u(const linear::u_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_c(const linear::c_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_init_register(const linear::init_register& instruction, std::ostream& output) = 0;
        virtual void assemble_load_parameter(const linear::load_parameter& instruction, std::ostream& output) = 0;
        virtual void assemble_load_effective_address(const linear::load_effective_address& instruction, std::ostream& output) = 0;
        virtual void assemble_load_memory(const linear::load_memory& instruction, std::ostream& output) = 0;
        virtual void assemble_store_memory(const linear::store_memory& instruction, std::ostream& output) = 0;
        virtual void assemble_alloca(const linear::alloca_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_valloca(const linear::valloca_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_branch(const linear::branch& instruction, std::ostream& output) = 0;
        virtual void assemble_branch_condition(const linear::branch_condition& instruction, std::ostream& output) = 0;
        virtual void assemble_push_function_argument(const linear::push_function_argument& instruction, std::ostream& output) = 0;
        virtual void assemble_function_call(const linear::function_call& instruction, std::ostream& output) = 0;
        virtual void assemble_function_return(const linear::function_return& instruction, std::ostream& output) = 0;
        virtual void assemble_phi(const linear::phi_instruction& instruction, std::ostream& output) = 0;
    };
}

#endif