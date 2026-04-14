#include "isa/x64.hpp"

using namespace michaelcc::isa::x64;

void x64_assembler::emit_unsigned_multiply(linear::register_info dest, linear::register_info operand_a, linear::register_info operand_b) {
    
}

void x64_assembler::dispatch(const linear::a_instruction& instruction) {
    auto physical_reg_dest = get_physical_register(instruction.destination());
    auto physical_reg_a = get_physical_register(instruction.operand_a());
    auto physical_reg_b = get_physical_register(instruction.operand_b());

    assert(instruction.operand_a().reg_size == instruction.operand_b().reg_size);
    assert(instruction.destination_register()->reg_size == instruction.operand_a().reg_size);

    begin_new_line();
    switch (instruction.type()) {
    case linear::MICHAELCC_LINEAR_A_ADD:
        if (instruction.operand_a() == instruction.destination()) { m_output << "add " << physical_reg_dest.name << ", " << physical_reg_b.name; }
        else if (instruction.operand_b() == instruction.destination()) { m_output << "add " << physical_reg_dest.name << ", " << physical_reg_a.name; }
        else { 
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
            m_output << "add " << physical_reg_dest.name << ", " << physical_reg_b.name;
        }
        break;
    case linear::MICHAELCC_LINEAR_A_SUBTRACT:
        if (instruction.operand_a() == instruction.destination()) { m_output << "sub " << physical_reg_dest.name << ", " << physical_reg_b.name; }
        else {
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
            m_output << "sub " << physical_reg_dest.name << ", " << physical_reg_b.name;
        }
        break;
    case linear::MICHAELCC_LINEAR_A_SIGNED_MULTIPLY:
        m_output << "push rax";
        begin_new_line();
        m_output << "push rbx";

    default: throw std::runtime_error("Invalid arithmetic/logical/comparison instruction type");
    }
}