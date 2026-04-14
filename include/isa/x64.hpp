#ifndef MICHAELCC_ISA_X64_HPP
#define MICHAELCC_ISA_X64_HPP

#include "platform.hpp"
#include "assembly/assembler.hpp"

namespace michaelcc::isa::x64 {
    extern michaelcc::platform_info platform_info;

    class x64_assembler : public michaelcc::assembly::assembler {
    public:
        x64_assembler(std::ostream& output) : michaelcc::assembly::assembler(output) {}

    protected:
        void begin_function_call(const linear::function_call& instruction, std::ostream& output) override;
        
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
}

#endif