#include "isa/x64.hpp"
#include "linear/registers.hpp"
#include <sys/resource.h>

using namespace michaelcc::isa::x64;

void x64_assembler::emit_unsigned_multiply(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    throw std::runtime_error("Unsigned multiply is not supported yet for x64");
}

void x64_assembler::emit_signed_multiply(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    throw std::runtime_error("Signed multiply is not supported yet for x64");
}

void x64_assembler::emit_signed_divide(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    throw std::runtime_error("Signed divide is not supported yet for x64");
}

void x64_assembler::emit_signed_remainder(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    throw std::runtime_error("Signed remainder is not supported yet for x64");
}

void x64_assembler::emit_unsigned_divide(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    throw std::runtime_error("Unsigned divide is not supported yet for x64");
}

void x64_assembler::emit_unsigned_remainder(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    throw std::runtime_error("Unsigned remainder is not supported yet for x64");
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

    // bitwise arithmetic
    case linear::MICHAELCC_LINEAR_A_BITWISE_AND:
        if (instruction.operand_a() == instruction.destination()) { m_output << "and " << physical_reg_dest.name << ", " << physical_reg_b.name; }
        else if (instruction.operand_b() == instruction.destination()) { m_output << "and " << physical_reg_dest.name << ", " << physical_reg_a.name; }
        else { 
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
            m_output << "and " << physical_reg_dest.name << ", " << physical_reg_b.name;
        }
        break;
    case linear::MICHAELCC_LINEAR_A_BITWISE_OR:
        if (instruction.operand_a() == instruction.destination()) { m_output << "or " << physical_reg_dest.name << ", " << physical_reg_b.name; }
        else if (instruction.operand_b() == instruction.destination()) { m_output << "or " << physical_reg_dest.name << ", " << physical_reg_a.name; }
        else { 
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
            m_output << "or " << physical_reg_dest.name << ", " << physical_reg_b.name;
        }
        break;
    case linear::MICHAELCC_LINEAR_A_BITWISE_XOR:
        if (instruction.operand_a() == instruction.destination()) { m_output << "xor " << physical_reg_dest.name << ", " << physical_reg_b.name; }
        else if (instruction.operand_b() == instruction.destination()) { m_output << "xor " << physical_reg_dest.name << ", " << physical_reg_a.name; }
        else { 
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
            m_output << "xor " << physical_reg_dest.name << ", " << physical_reg_b.name;
        }
        break;
    
    // boolean arithmetic
        

    // float arithmetic
    case linear::MICHAELCC_LINEAR_A_FLOAT_ADD:
        if (instruction.destination().reg_size == linear::word_size::MICHAELCC_WORD_SIZE_UINT64) {
            if (instruction.operand_a() == instruction.destination()) { m_output << "addsd " << physical_reg_dest.name << ", " << physical_reg_b.name; }
            else if (instruction.operand_b() == instruction.destination()) { m_output << "addsd " << physical_reg_dest.name << ", " << physical_reg_a.name; }
            else { 
                m_output << "movsd " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
                m_output << "addsd " << physical_reg_dest.name << ", " << physical_reg_b.name;
            }
        } else {
            if (instruction.operand_a() == instruction.destination()) { m_output << "addss " << physical_reg_dest.name << ", " << physical_reg_b.name; }
            else if (instruction.operand_b() == instruction.destination()) { m_output << "addss " << physical_reg_dest.name << ", " << physical_reg_a.name; }
            else { 
                m_output << "movss " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
                m_output << "addss " << physical_reg_dest.name << ", " << physical_reg_b.name;
            }
        }
        break;
    case linear::MICHAELCC_LINEAR_A_FLOAT_SUBTRACT:
        if (instruction.destination().reg_size == linear::word_size::MICHAELCC_WORD_SIZE_UINT64) {
            if (instruction.operand_a() != instruction.destination()) {
                m_output << "movsd " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
            }
            m_output << "subsd " << physical_reg_dest.name << ", " << physical_reg_b.name;
        } else {
            if (instruction.operand_a() != instruction.destination()) {
                m_output << "movss " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
            }
            m_output << "subss " << physical_reg_dest.name << ", " << physical_reg_b.name;
        }
        break;
    case linear::MICHAELCC_LINEAR_A_FLOAT_MULTIPLY:
        if (instruction.destination().reg_size == linear::word_size::MICHAELCC_WORD_SIZE_UINT64) {
            if (instruction.operand_a() == instruction.destination()) { m_output << "mulsd " << physical_reg_dest.name << ", " << physical_reg_b.name; }
            else if (instruction.operand_b() == instruction.destination()) { m_output << "mulsd " << physical_reg_dest.name << ", " << physical_reg_a.name; }
            else { 
                m_output << "movsd " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
                m_output << "mulsd " << physical_reg_dest.name << ", " << physical_reg_b.name;
            }
        } else {
            if (instruction.operand_a() == instruction.destination()) { m_output << "mulss " << physical_reg_dest.name << ", " << physical_reg_b.name; }
            else if (instruction.operand_b() == instruction.destination()) { m_output << "mulss " << physical_reg_dest.name << ", " << physical_reg_a.name; }
            else { 
                m_output << "movss " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
                m_output << "mulss " << physical_reg_dest.name << ", " << physical_reg_b.name;
            }
        }
        break;
    case linear::MICHAELCC_LINEAR_A_FLOAT_DIVIDE:
        if (instruction.destination().reg_size == linear::word_size::MICHAELCC_WORD_SIZE_UINT64) {
            if (instruction.operand_a() == instruction.destination()) { m_output << "divsd " << physical_reg_dest.name << ", " << physical_reg_b.name; }
            else if (instruction.operand_b() == instruction.destination()) { m_output << "divsd " << physical_reg_dest.name << ", " << physical_reg_a.name; }
            else { 
                m_output << "movsd " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
                m_output << "divsd " << physical_reg_dest.name << ", " << physical_reg_b.name;
            }
        } else {
            if (instruction.operand_a() == instruction.destination()) { m_output << "divss " << physical_reg_dest.name << ", " << physical_reg_b.name; }
            else if (instruction.operand_b() == instruction.destination()) { m_output << "divss " << physical_reg_dest.name << ", " << physical_reg_a.name; }
            else { 
                m_output << "movss " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
                m_output << "divss " << physical_reg_dest.name << ", " << physical_reg_b.name;
            }
        }
        break;
    case linear::MICHAELCC_LINEAR_A_FLOAT_MODULO:
        throw std::runtime_error("Float modulo is not supported yet for x64. Just use an fmod or subtract quotient manually.");
    
    default: throw std::runtime_error("Invalid arithmetic/logical/comparison instruction type");
    }
}

void x64_assembler::dispatch(const linear::a2_instruction& instruction) {
    auto physical_reg_dest = get_physical_register(instruction.destination());
    auto physical_reg_a = get_physical_register(instruction.operand_a());
    
    begin_new_line();
    switch (instruction.type()) {
    case linear::MICHAELCC_LINEAR_A_ADD:
        if (instruction.destination() != instruction.operand_a()) {
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
        }
        m_output << "add " << physical_reg_dest.name << ", " << instruction.constant();
        break;
    case linear::MICHAELCC_LINEAR_A_SUBTRACT:
        if (instruction.destination() != instruction.operand_a()) {
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
        }
        m_output << "sub " << physical_reg_dest.name << ", " << instruction.constant();
        break;
    case linear::MICHAELCC_LINEAR_A_SHIFT_LEFT:
        if (instruction.destination() != instruction.operand_a()) {
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
        }
        m_output << "shl " << physical_reg_dest.name << ", " << instruction.constant();
        break;
    case linear::MICHAELCC_LINEAR_A_SIGNED_SHIFT_RIGHT:
        if (instruction.destination() != instruction.operand_a()) {
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
        }
        m_output << "sar " << physical_reg_dest.name << ", " << instruction.constant();
        break;
    case linear::MICHAELCC_LINEAR_A_UNSIGNED_SHIFT_RIGHT:
        if (instruction.destination() != instruction.operand_a()) {
            m_output << "mov " << physical_reg_dest.name << ", " << physical_reg_a.name;
            begin_new_line();
        }
        m_output << "shr " << physical_reg_dest.name << ", " << instruction.constant();
        break;
    default: throw std::runtime_error("Invalid arithmetic/logical/comparison instruction type");
    }
}