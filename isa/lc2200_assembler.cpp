#include "isa/lc2200.hpp"
#include "linear/ir.hpp"

void michaelcc::isa::lc2200::lc2200_assembler::begin_block_preamble(const linear::basic_block& block) {

}

void michaelcc::isa::lc2200::lc2200_assembler::begin_function_preamble(const linear::function_definition& definition) {

}

void michaelcc::isa::lc2200::lc2200_assembler::begin_function_call(const linear::function_call& instruction) {

}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::a_instruction& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_a = get_physical_register(instruction.operand_a());
    auto physical_b = get_physical_register(instruction.operand_b());

    begin_new_line();
    switch (instruction.type()) {
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_ADD:
        m_output << "add " << physical_destination.name << ", " << physical_a.name << ", " << physical_b.name;
        break;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_SUBTRACT:
        // negate b and put in dest
        m_output << "nand " << physical_destination.name << ", " << physical_b.name << ", " << physical_b.name;
        begin_new_line();
        m_output << "addi " << physical_destination.name << ", " << physical_destination.name << ", 1";

        // add a and negated b (in dest) to dest
        begin_new_line();
        m_output << "add " << physical_destination.name << ", " << physical_destination.name << ", " << physical_a.name;
        break;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_BITWISE_AND:
        m_output << "nand " << physical_destination.name << ", " << physical_a.name << ", " << physical_b.name;
        begin_new_line();
        m_output << "nand " << physical_destination.name << ", " << physical_destination.name << ", " << physical_destination.name;
        break;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_BITWISE_NAND:
        m_output << "nand " << physical_destination.name << ", " << physical_a.name << ", " << physical_b.name;
        break;
    default: throw std::runtime_error("Invalid a instruction type");
    }
}