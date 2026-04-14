#include "isa/lc2200.hpp"
#include "linear/ir.hpp"

void michaelcc::isa::lc2200::lc2200_assembler::begin_block_preamble(const linear::basic_block& block) {

}

void michaelcc::isa::lc2200::lc2200_assembler::begin_function_preamble(const linear::function_definition& definition) {

}

void michaelcc::isa::lc2200::lc2200_assembler::begin_function_call(const linear::function_call& instruction) {

}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::a_instruction& instruction) {
    switch (instruction.type()) {
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_AND:
        emit_logical_and(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    default: break;
    };

    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_a = get_physical_register(instruction.operand_a());
    auto physical_b = get_physical_register(instruction.operand_b());

    begin_new_line();
    switch (instruction.type()) {
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_ADD:
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_OR:
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

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::a2_instruction& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_a = get_physical_register(instruction.operand_a());

    begin_new_line();
    switch (instruction.type()) {
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_ADD:
        m_output << "addi " << physical_destination.name << ", " << physical_a.name << ", " << instruction.constant();
        break;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_SUBTRACT: {
        m_output << "addi " << physical_destination.name << ", " << physical_a.name << ", -" << instruction.constant();
        break;
    }
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_SHIFT_LEFT:{
        if (instruction.constant() < 1) { break; }

        m_output << "add " << physical_destination.name << ", " << physical_a.name << ", " << physical_a.name;
        for (size_t i = 0; i < instruction.constant(); i++) {
            begin_new_line();
            m_output << "add " << physical_destination.name << ", " << physical_destination.name << ", " << physical_destination.name;
        }
        break;
    }
    default: throw std::runtime_error("Invalid a2 instruction type");
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::u_instruction& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_operand = get_physical_register(instruction.operand());

    begin_new_line();
    switch (instruction.type()) {
    case linear::u_instruction_type::MICHAELCC_LINEAR_U_NEGATE:
        m_output << "nand " << physical_destination.name << ", " << physical_operand.name << ", " << physical_operand.name;
        begin_new_line();
        m_output << "addi " << physical_destination.name << ", " << physical_destination.name << ", 1";
        break;
    case linear::u_instruction_type::MICHAELCC_LINEAR_U_BITWISE_NOT:
        m_output << "nand " << physical_destination.name << ", " << physical_operand.name << ", " << physical_operand.name;
        break;
    default: throw std::runtime_error("Invalid u instruction type");
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::c_instruction& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_source = get_physical_register(instruction.source());

    begin_new_line();
    switch (instruction.type()) {
    case michaelcc::linear::MICHAELCC_LINEAR_C_COPY_INIT:
        m_output << "add " << physical_destination.name << ", " << physical_source.name << ", $zero";
        break;
    default: throw std::runtime_error("Invalid c instruction type");
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::init_register& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_value = instruction.value();

    begin_new_line();
    m_output << "add " << physical_destination.name << ", $zero, ";

    switch (instruction.destination().reg_size) {
    case michaelcc::linear::MICHAELCC_WORD_SIZE_BYTE:
        m_output << physical_value.ubyte;
        break;
    case michaelcc::linear::MICHAELCC_WORD_SIZE_UINT16:
        m_output << physical_value.uint16;
        break;
    case michaelcc::linear::MICHAELCC_WORD_SIZE_UINT32:
        m_output << physical_value.uint32;
        break;
    case michaelcc::linear::MICHAELCC_WORD_SIZE_UINT64:
        m_output << physical_value.uint64;
        break;
    default: throw std::runtime_error("Invalid init register value type");
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::load_memory& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_source_address = get_physical_register(instruction.source_address());

    begin_new_line();
    m_output << "lw " << physical_destination.name << ", " << "0x" << instruction.offset();
    m_output << "(" << physical_source_address.name << ");";
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::store_memory& instruction) {
    auto physical_value = get_physical_register(instruction.value());
    auto physical_destination_address = get_physical_register(instruction.destination_address());

    begin_new_line();
    m_output << "sw " << physical_value.name << ", " << "0x" << instruction.offset();
    m_output << "(" << physical_destination_address.name << ");";
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::load_effective_address& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());

    begin_new_line();
    m_output << "lea " << physical_destination.name << ", " << instruction.label();
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::valloca_instruction& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_size = get_physical_register(instruction.size());

    //push size to stack (recall stack grows downward)
    //alignment doesnt matter cause in LC-4 max alignment is 4 bytes which is the same as the word size

    // negate size and put in dest
    begin_new_line();
    m_output << "nand" << physical_destination.name << ", " << physical_size.name << ", " << physical_size.name;
    begin_new_line();
    m_output << "addi " << physical_destination.name << ", " << physical_destination.name << ", 1";


    // add negated size to stack
    begin_new_line();
    m_output << "add $sp, $sp, " << physical_destination.name;

    // move stack pointer to destination
    begin_new_line();
    m_output << "add " << physical_destination.name << ", $sp, $zero";
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::branch& instruction) {
    m_output << "beq $zero, $zero, block" << instruction.next_block_id();
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::branch_condition& instruction) {
    auto physical_condition = get_physical_register(instruction.condition());

    begin_new_line();
    m_output << "beq " << physical_condition.name << ", $zero, block" << instruction.if_false_block_id();
    begin_new_line();
    m_output << "beq $zero, $zero, block" << instruction.if_true_block_id();
}