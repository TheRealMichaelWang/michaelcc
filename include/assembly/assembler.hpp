#ifndef MICHAELCC_ASSEMBLY_ASSEMBLER_HPP
#define MICHAELCC_ASSEMBLY_ASSEMBLER_HPP

#include "linear/ir.hpp"

namespace michaelcc::assembly {
    class assembler {
    public:
        virtual ~assembler() = default;

        virtual void assemble_arithmetic(linear::a_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_a2(linear::a2_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_u(linear::u_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_c(linear::c_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_init_register(linear::init_register& instruction, std::ostream& output) = 0;
        virtual void assemble_load_parameter(linear::load_parameter& instruction, std::ostream& output) = 0;
        virtual void assemble_load_effective_address(linear::load_effective_address& instruction, std::ostream& output) = 0;
        virtual void assemble_load_memory(linear::load_memory& instruction, std::ostream& output) = 0;
        virtual void assemble_store_memory(linear::store_memory& instruction, std::ostream& output) = 0;
        virtual void assemble_alloca(linear::alloca_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_valloca(linear::valloca_instruction& instruction, std::ostream& output) = 0;
        virtual void assemble_branch(linear::branch& instruction, std::ostream& output) = 0;
        virtual void assemble_branch_condition(linear::branch_condition& instruction, std::ostream& output) = 0;
        virtual void assemble_push_function_argument(linear::push_function_argument& instruction, std::ostream& output) = 0;
        virtual void assemble_function_call(linear::function_call& instruction, std::ostream& output) = 0;
        virtual void assemble_function_return(linear::function_return& instruction, std::ostream& output) = 0;
        virtual void assemble_phi(linear::phi_instruction& instruction, std::ostream& output) = 0;
    };
}

#endif