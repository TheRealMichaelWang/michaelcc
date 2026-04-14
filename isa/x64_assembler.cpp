#include "isa/x64.hpp"
#include "linear/registers.hpp"
#include <sys/resource.h>

using namespace michaelcc::isa::x64;

void x64_assembler::emit_unsigned_multiply(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    throw std::runtime_error("Unsigned multiply is not supported yet for x64");
}

void x64_assembler::dispatch(const linear::a_instruction& instruction) {
    switch (instruction.type()) {
    case linear::MICHAELCC_LINEAR_A_ADD:
        emit_unsigned_multiply(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        break;
    case linear::MICHAELCC_LINEAR_A_SUBTRACT:
        emit_unsigned_multiply(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        break;
    case linear::MICHAELCC_LINEAR_A_SIGNED_MULTIPLY:
        emit_signed_multiply(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        break;
    case linear::MICHAELCC_LINEAR_A_SIGNED_DIVIDE:
        emit_signed_divide(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        break;
    case linear::MICHAELCC_LINEAR_A_SIGNED_MODULO:
        emit_signed_remainder(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        break;
    case linear::MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY:
        emit_unsigned_multiply(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        break;
    case linear::MICHAELCC_LINEAR_A_UNSIGNED_DIVIDE:
        emit_unsigned_divide(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        break;
    case linear::MICHAELCC_LINEAR_A_UNSIGNED_MODULO:
        emit_unsigned_remainder(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        break;
        
    default:
        break;
    }

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
        if (instruction.operand_a() != instruction.destination()) { 
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
        }
        m_output << "sub " << physical_reg_dest.name << ", " << physical_reg_b.name;
        break;
    default: throw std::runtime_error("Invalid arithmetic/logical/comparison instruction type");
    }
}