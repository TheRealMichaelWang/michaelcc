#include "isa/lc2200.hpp"
#include "linear/ir.hpp"
#include "linear/registers.hpp"
#include "utils.hpp"
#include <vector>
#include <variant>

// how much to subtract from the frame pointer to get to the last parameter
const size_t fp_to_parameter_offset = 2;

void michaelcc::isa::lc2200::lc2200_assembler::begin_block_preamble(const linear::basic_block& block) {
    if (m_block_preamble_infos.contains(block.id())) {
        auto& block_preamble_info = m_block_preamble_infos.at(block.id());
        for (auto& virtual_register : block_preamble_info.to_zero) {
            begin_new_line();
            auto physical_register = get_physical_register(virtual_register);
            m_output << "add " << physical_register.name << ", $zero, $zero";
        }
        for (auto& virtual_register : block_preamble_info.to_set_to_one) {
            begin_new_line();
            auto physical_register = get_physical_register(virtual_register);
            m_output << "addi " << physical_register.name << ", $zero, 1";
        }
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::begin_function_preamble(const linear::function_definition& definition) {
    //push old fp to the stack
    begin_new_line();
    m_output << "addi $sp, $sp, -1";
    write_comment("save old frame pointer");
    begin_new_line();
    m_output << "sw $fp, 0($sp)";

    // set fp to current sp
    begin_new_line();
    m_output << "addi $fp, $sp, 0";
    write_comment("set frame pointer to current stack pointer");

    // save all callee saved registers
    size_t i = 0;
    for (auto& register_info : m_current_unit.value()->platform_info.registers) {
        if (register_info.is_callee_saved && !register_info.is_protected) {
            begin_new_line();
            i++;
            m_output << "sw " << register_info.name << ", -" << i << "($sp)";

            write_comment(std::format("saved callee saved register {}", register_info.name));
        }
    }
    if (i > 0) {
        begin_new_line(); //update sp
        m_output << "addi $sp, $sp, -" << i;
    }
    
    //reserve space for locals
    auto& frame_allocator = *m_current_frame_allocator.value();
    size_t reserved_stack_space = frame_allocator.get_reserved_stack_space(definition.entry_block_id());    
    
    begin_new_line();
    m_output << "addi $sp, $sp, -" << reserved_stack_space;
    write_comment("reserve space for locals");
}

void michaelcc::isa::lc2200::lc2200_assembler::begin_function_call(const linear::function_call& instruction) {
    std::vector<linear::register_t> physical_registers_to_save;
    for (auto vreg : instruction.caller_saved_registers()) {
        auto physical_register = get_physical_register(vreg);
        assert(physical_register.size == michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32);

        if (!physical_register.is_caller_saved) {
            continue;
        }

        physical_registers_to_save.push_back(physical_register.id);
    }

    std::unordered_map<linear::register_t, size_t> caller_saved_registers_offsets;
    size_t pushed_register_size = physical_registers_to_save.size();
    for (size_t i = 0; i < physical_registers_to_save.size(); i++) {
        auto& reg_info = m_current_unit.value()->platform_info.get_register_info(physical_registers_to_save[i]);
        begin_new_line();
        m_output << "sw " << reg_info.name << ", -" << (i + 1) << "($sp)";
        write_comment(std::format("saved caller saved register {}", reg_info.name));

        size_t sp_subtract_offset = physical_registers_to_save.size() - i - 1;
        caller_saved_registers_offsets.insert({ physical_registers_to_save[i], sp_subtract_offset });
    }

    if (physical_registers_to_save.size() > 0) {
        begin_new_line();
        m_output << "addi $sp, $sp, -" << physical_registers_to_save.size();
    }

    m_function_call_infos.insert({ 
        instruction.function_call_id(), 
        function_call_info{ 
            std::move(caller_saved_registers_offsets), 
            {} ,
            0,
            pushed_register_size
        } 
    });
}

void michaelcc::isa::lc2200::lc2200_assembler::emit_multiplication(linear::virtual_register destination, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_destination = get_physical_register(destination);
    auto physical_a = get_physical_register(operand_a);
    auto physical_b = get_physical_register(operand_b);

    auto loop_label = generate_symbol();
    auto skip_label = generate_symbol();
    auto done_label = generate_symbol();

    begin_new_line();
    m_output << "addi $sp, $sp, -4";
    write_comment(std::format("begin multiplication of {} and {}", physical_a.name, physical_b.name));
    begin_new_line();
    m_output << "sw " << physical_a.name << ", 3($sp)";
    begin_new_line();
    m_output << "sw " << physical_b.name << ", 2($sp)";
    begin_new_line();
    m_output << "addi $at, $zero, 1";
    begin_new_line();
    m_output << "sw $at, 1($sp)";
    begin_new_line();
    m_output << "sw $zero, 0($sp)";

    emit_label(loop_label);
    begin_new_line();
    m_output << "lw $at, 1($sp)";
    begin_new_line();
    m_output << "beq $at, $zero, " << done_label;
    begin_new_line();
    m_output << "lw " << physical_destination.name << ", 3($sp)";
    begin_new_line();
    m_output << "nand $at, " << physical_destination.name << ", $at";
    begin_new_line();
    m_output << "nand $at, $at, $at";
    begin_new_line();
    m_output << "beq $at, $zero, " << skip_label;
    begin_new_line();
    m_output << "lw " << physical_destination.name << ", 0($sp)";
    begin_new_line();
    m_output << "lw $at, 2($sp)";
    begin_new_line();
    m_output << "add " << physical_destination.name << ", " << physical_destination.name << ", $at";
    begin_new_line();
    m_output << "sw " << physical_destination.name << ", 0($sp)";

    emit_label(skip_label);
    begin_new_line();
    m_output << "lw $at, 2($sp)";
    begin_new_line();
    m_output << "add $at, $at, $at";
    begin_new_line();
    m_output << "sw $at, 2($sp)";
    begin_new_line();
    m_output << "lw $at, 1($sp)";
    begin_new_line();
    m_output << "add $at, $at, $at";
    begin_new_line();
    m_output << "sw $at, 1($sp)";
    begin_new_line();
    m_output << "beq $zero, $zero, " << loop_label;

    emit_label(done_label);
    begin_new_line();
    m_output << "lw " << physical_destination.name << ", 0($sp)";
    begin_new_line();
    m_output << "addi $sp, $sp, 4";
    write_comment(std::format("end multiplication of {} and {}", physical_a.name, physical_b.name));
}

void michaelcc::isa::lc2200::lc2200_assembler::emit_compare_equal(linear::virtual_register destination, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_destination = get_physical_register(destination);
    auto physical_a = get_physical_register(operand_a);
    auto physical_b = get_physical_register(operand_b);

    if(next_instruction().has_value()) {
        if (auto* branch_condition = dynamic_cast<const linear::branch_condition*>(next_instruction().value())) {
            if (branch_condition->condition() == destination) {
                begin_new_line();
                m_output << "beq " << physical_a.name << ", " << physical_b.name << ", block" << branch_condition->if_true_block_id();
                
                if (!schedule_block_next(branch_condition->if_false_block_id())) {
                    begin_new_line();
                    m_output << "beq $zero, $zero, block" << branch_condition->if_false_block_id();
                }

                block_add_to_set_to_one(branch_condition->if_true_block_id(), destination);
                block_add_to_zero(branch_condition->if_false_block_id(), destination);
                return;
            }
        }
    }

    auto eq_label = generate_symbol();
    auto end_label = generate_symbol();

    begin_new_line();
    m_output << "beq " << physical_a.name << ", " << physical_b.name << ", " << eq_label;
    begin_new_line();
    m_output << "add " << physical_destination.name << ", $zero, $zero";
    begin_new_line();
    m_output << "beq $zero, $zero, " << end_label;
    emit_label(eq_label);
    begin_new_line();
    m_output << "addi " << physical_destination.name << ", $zero, 1";
    emit_label(end_label);
}

void michaelcc::isa::lc2200::lc2200_assembler::emit_compare_not_equal(linear::virtual_register destination, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_destination = get_physical_register(destination);
    auto physical_a = get_physical_register(operand_a);
    auto physical_b = get_physical_register(operand_b);

    if (next_instruction().has_value()) {
        if (auto* branch_condition = dynamic_cast<const linear::branch_condition*>(next_instruction().value())) {
            if (branch_condition->condition() == destination) {
                begin_new_line();
                m_output << "beq " << physical_a.name << ", " << physical_b.name << ", block" << branch_condition->if_false_block_id();
                
                if (!schedule_block_next(branch_condition->if_true_block_id())) {
                    begin_new_line();
                    m_output << "beq $zero, $zero, block" << branch_condition->if_true_block_id();
                }
                
                block_add_to_set_to_one(branch_condition->if_false_block_id(), destination);
                block_add_to_zero(branch_condition->if_true_block_id(), destination);
                return;
            }
        }
    }
    
    auto ne_label = generate_symbol();
    auto end_label = generate_symbol();
    
    begin_new_line();
    m_output << "beq " << physical_a.name << ", " << physical_b.name << ", " << ne_label;
    begin_new_line();
    m_output << "addi " << physical_destination.name << ", $zero, 1";
    begin_new_line();
    m_output << "beq $zero, $zero, " << end_label;
    emit_label(ne_label);
    begin_new_line();
    m_output << "add " << physical_destination.name << ", $zero, $zero";
    emit_label(end_label);
}

void michaelcc::isa::lc2200::lc2200_assembler::emit_logical_and(linear::virtual_register destination, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_destination = get_physical_register(destination);
    auto physical_a = get_physical_register(operand_a);
    auto physical_b = get_physical_register(operand_b);

    if (next_instruction().has_value()) {
        if (auto* branch_condition = dynamic_cast<const linear::branch_condition*>(next_instruction().value())) {
            if (branch_condition->condition() == destination) {
                begin_new_line();
                m_output << "beq " << physical_a.name << ", $zero, block" << branch_condition->if_false_block_id();
                begin_new_line();
                m_output << "beq " << physical_b.name << ", $zero, block" << branch_condition->if_false_block_id();

                if (!schedule_block_next(branch_condition->if_true_block_id())) {
                    begin_new_line();
                    m_output << "beq $zero, $zero, block" << branch_condition->if_true_block_id();
                }

                block_add_to_set_to_one(branch_condition->if_true_block_id(), destination);
                block_add_to_zero(branch_condition->if_false_block_id(), destination);
                return;
            }
        }
    }

    auto false_label = generate_symbol();
    auto end_label = generate_symbol();

    begin_new_line();
    m_output << "beq " << physical_a.name << ", $zero, " << false_label;
    begin_new_line();
    m_output << "beq " << physical_b.name << ", $zero, " << false_label;
    begin_new_line();
    m_output << "addi " << physical_destination.name << ", $zero, 1";
    begin_new_line();
    m_output << "beq $zero, $zero, " << end_label;
    emit_label(false_label);
    begin_new_line();
    m_output << "add " << physical_destination.name << ", $zero, $zero";
    emit_label(end_label);
}

void michaelcc::isa::lc2200::lc2200_assembler::emit_logical_or(linear::virtual_register destination, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_destination = get_physical_register(destination);
    auto physical_a = get_physical_register(operand_a);
    auto physical_b = get_physical_register(operand_b);

    if (next_instruction().has_value()) {
        if (auto* branch_condition = dynamic_cast<const linear::branch_condition*>(next_instruction().value())) {
            if (branch_condition->condition() == destination) {
                auto check_b_label = generate_symbol();

                begin_new_line();
                m_output << "beq " << physical_a.name << ", $zero, " << check_b_label;
                begin_new_line();
                m_output << "beq $zero, $zero, block" << branch_condition->if_true_block_id();
                emit_label(check_b_label);
                begin_new_line();
                m_output << "beq " << physical_b.name << ", $zero, block" << branch_condition->if_false_block_id();

                if (!schedule_block_next(branch_condition->if_true_block_id())) {
                    begin_new_line();
                    m_output << "beq $zero, $zero, block" << branch_condition->if_true_block_id();
                }

                block_add_to_set_to_one(branch_condition->if_true_block_id(), destination);
                block_add_to_zero(branch_condition->if_false_block_id(), destination);
                return;
            }
        }
    }

    auto check_b_label = generate_symbol();
    auto true_label = generate_symbol();
    auto false_label = generate_symbol();
    auto end_label = generate_symbol();

    begin_new_line();
    m_output << "beq " << physical_a.name << ", $zero, " << check_b_label;
    begin_new_line();
    m_output << "beq $zero, $zero, " << true_label;
    emit_label(check_b_label);
    begin_new_line();
    m_output << "beq " << physical_b.name << ", $zero, " << false_label;
    emit_label(true_label);
    begin_new_line();
    m_output << "addi " << physical_destination.name << ", $zero, 1";
    begin_new_line();
    m_output << "beq $zero, $zero, " << end_label;
    emit_label(false_label);
    begin_new_line();
    m_output << "add " << physical_destination.name << ", $zero, $zero";
    emit_label(end_label);
}

void michaelcc::isa::lc2200::lc2200_assembler::emit_compare_greater_than(linear::virtual_register destination, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_destination = get_physical_register(destination);
    auto physical_a = get_physical_register(operand_a);
    auto physical_b = get_physical_register(operand_b);

    if (next_instruction().has_value()) {
        if (auto* branch_condition = dynamic_cast<const linear::branch_condition*>(next_instruction().value())) {
            if (branch_condition->condition() == destination) {
                begin_new_line();
                m_output << "bgt " << physical_a.name << ", " << physical_b.name << ", block" << branch_condition->if_true_block_id();

                if (!schedule_block_next(branch_condition->if_false_block_id())) {
                    begin_new_line();
                    m_output << "beq $zero, $zero, block" << branch_condition->if_false_block_id();
                }

                block_add_to_set_to_one(branch_condition->if_true_block_id(), destination);
                block_add_to_zero(branch_condition->if_false_block_id(), destination);
                return;
            }
        }
    }

    auto gt_label = generate_symbol();
    auto end_label = generate_symbol();

    begin_new_line();
    m_output << "bgt " << physical_a.name << ", " << physical_b.name << ", " << gt_label;
    begin_new_line();
    m_output << "add " << physical_destination.name << ", $zero, $zero";
    begin_new_line();
    m_output << "beq $zero, $zero, " << end_label;
    emit_label(gt_label);
    begin_new_line();
    m_output << "addi " << physical_destination.name << ", $zero, 1";
    emit_label(end_label);
}

void michaelcc::isa::lc2200::lc2200_assembler::emit_compare_greater_than_or_equal(linear::virtual_register destination, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_destination = get_physical_register(destination);
    auto physical_a = get_physical_register(operand_a);
    auto physical_b = get_physical_register(operand_b);

    if (next_instruction().has_value()) {
        if (auto* branch_condition = dynamic_cast<const linear::branch_condition*>(next_instruction().value())) {
            if (branch_condition->condition() == destination) {
                begin_new_line();
                m_output << "bgt " << physical_a.name << ", " << physical_b.name << ", block" << branch_condition->if_true_block_id();
                begin_new_line();
                m_output << "beq " << physical_a.name << ", " << physical_b.name << ", block" << branch_condition->if_true_block_id();

                if (!schedule_block_next(branch_condition->if_false_block_id())) {
                    begin_new_line();
                    m_output << "beq $zero, $zero, block" << branch_condition->if_false_block_id();
                }

                block_add_to_set_to_one(branch_condition->if_true_block_id(), destination);
                block_add_to_zero(branch_condition->if_false_block_id(), destination);
                return;
            }
        }
    }

    auto gte_label = generate_symbol();
    auto end_label = generate_symbol();

    begin_new_line();
    m_output << "bgt " << physical_a.name << ", " << physical_b.name << ", " << gte_label;
    begin_new_line();
    m_output << "beq " << physical_a.name << ", " << physical_b.name << ", " << gte_label;
    begin_new_line();
    m_output << "add " << physical_destination.name << ", $zero, $zero";
    begin_new_line();
    m_output << "beq $zero, $zero, " << end_label;
    emit_label(gte_label);
    begin_new_line();
    m_output << "addi " << physical_destination.name << ", $zero, 1";
    emit_label(end_label);
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::a_instruction& instruction) {
    switch (instruction.type()) {
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_SIGNED_MULTIPLY:
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY:
        emit_multiplication(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_EQUAL:
        emit_compare_equal(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL:
        emit_compare_not_equal(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_AND:
        emit_logical_and(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_OR:
        emit_logical_or(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN:
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN:
        emit_compare_greater_than(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN_OR_EQUAL:
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN_OR_EQUAL:
        emit_compare_greater_than_or_equal(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN:
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN:
        emit_compare_greater_than(instruction.destination(), instruction.operand_b(), instruction.operand_a());
        return;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN_OR_EQUAL:
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN_OR_EQUAL:
        emit_compare_greater_than_or_equal(instruction.destination(), instruction.operand_b(), instruction.operand_a());
        return;
    default:
        break; 
    }

    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_a = get_physical_register(instruction.operand_a());
    auto physical_b = get_physical_register(instruction.operand_b());

    begin_new_line();
    switch (instruction.type()) {
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_ADD:
        m_output << "add " << physical_destination.name << ", " << physical_a.name << ", " << physical_b.name;
        break;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_SUBTRACT:
        // negate b into $at (scratch), safe even when dest aliases a or b
        m_output << "nand $at, " << physical_b.name << ", " << physical_b.name;
        begin_new_line();
        m_output << "addi $at, $at, 1";
        begin_new_line();
        m_output << "add " << physical_destination.name << ", " << physical_a.name << ", $at";
        break;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_BITWISE_AND:
        m_output << "nand " << physical_destination.name << ", " << physical_a.name << ", " << physical_b.name;
        begin_new_line();
        m_output << "nand " << physical_destination.name << ", " << physical_destination.name << ", " << physical_destination.name;
        break;
    case linear::a_instruction_type::MICHAELCC_LINEAR_A_BITWISE_NAND:
        m_output << "nand " << physical_destination.name << ", " << physical_a.name << ", " << physical_b.name;
        break;
    default: 
        throw std::runtime_error("Invalid a instruction type");
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
    m_output << "addi " << physical_destination.name << ", $zero, ";

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
    m_output << "lw " << physical_destination.name << ", " << instruction.offset();
    m_output << "(" << physical_source_address.name << ")";
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::store_memory& instruction) {
    auto physical_value = get_physical_register(instruction.value());
    auto physical_destination_address = get_physical_register(instruction.destination_address());

    begin_new_line();
    m_output << "sw " << physical_value.name << ", " << instruction.offset();
    m_output << "(" << physical_destination_address.name << ")";
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::load_effective_address& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());

    begin_new_line();
    m_output << "lea " << physical_destination.name << ", " << instruction.label();
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::load_parameter& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    if (instruction.parameter().pass_via_stack()) {
        // load a stack alloced objects address into the physical destination register
        begin_new_line();
        m_output << "addi " << physical_destination.name << ", $fp, -" << (instruction.parameter().offset.value() + fp_to_parameter_offset);
    } else if (instruction.parameter().pass_via_register.has_value()){
        assert(physical_destination.id == instruction.parameter().pass_via_register.value());
        // do nothing because the parameter is already in the physical destination register
    } else {
        assert(instruction.parameter().register_class.has_value());

        // load the parameter into register from the stack
        begin_new_line();
        m_output << "lw " << physical_destination.name << ", " << (instruction.parameter().offset.value() + fp_to_parameter_offset) << "($fp)";
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::valloca_instruction& instruction) {
    auto physical_destination = get_physical_register(instruction.destination());
    auto physical_size = get_physical_register(instruction.size());

    //push size to stack (recall stack grows downward)
    //alignment doesnt matter cause in LC-4 max alignment is 4 bytes which is the same as the word size

    // negate size and put in dest
    begin_new_line();
    m_output << "nand " << physical_destination.name << ", " << physical_size.name << ", " << physical_size.name;
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

    if (!schedule_block_next(instruction.if_true_block_id())) {
        begin_new_line();
        m_output << "beq $zero, $zero, block" << instruction.if_true_block_id();
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::push_function_argument& instruction) {
    auto& function_call_info = m_function_call_infos.at(instruction.function_call_id());
    auto argument_physical_register = get_physical_register(instruction.value());

    if (instruction.argument().pass_via_stack()) { //save argument onto stack
        if (instruction.argument().register_class.has_value()) { //this is a register on the stack
            begin_new_line();
            m_output << "sw " << argument_physical_register.name << ", -" << (instruction.argument().offset.value() + 1) << "($sp)";
            write_comment("saved argument to stack");
        } else {
            //manual memcopy to stack
            for (size_t i = 0; i < instruction.argument().layout.size; i++) {
                // at is the ultimate scratchpad register
                begin_new_line();
                m_output << "lw " << "$at, " << i << '(' << argument_physical_register.name << ')';
                write_comment(std::format("copying word {}/{} of argument onto stack", i, instruction.argument().layout.size));
                begin_new_line();
                m_output << "sw " << "$at, -" << ((instruction.argument().offset.value() + 1) - i) << "($sp)";
            }
        }
        function_call_info.pushed_parameter_size = std::max(function_call_info.pushed_parameter_size, instruction.argument().offset.value() + 1);
    } else {
        auto physical_argument_register = m_current_unit.value()->platform_info.get_register_info(instruction.argument().pass_via_register.value());
        // read from arg dest register into assigned a register
        if (function_call_info.trashed_registers.contains(argument_physical_register.id)) {
            // good thing v0 isn't used and is protected
            if (argument_physical_register.id != physical_argument_register.id) {
                begin_new_line();
                m_output << "lw " << "$at, " << function_call_info.caller_saved_registers_offsets.at(argument_physical_register.id) << "($sp)";
                begin_new_line();
                m_output << "add " << physical_argument_register.name << ", $zero, $at";
            }
        } else {
            // read from arg dest register into assigned a register
            begin_new_line();
            m_output << "add " << physical_argument_register.name << ", " << argument_physical_register.name << ", $zero";
            function_call_info.trashed_registers.insert(instruction.argument().pass_via_register.value());
        }
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::function_call& instruction) {
    auto function_call_info = m_function_call_infos.at(instruction.function_call_id());

    if (function_call_info.pushed_parameter_size > 0) { //finalize push parameters
        begin_new_line();
        m_output << "addi $sp, $sp, -" << function_call_info.pushed_parameter_size;
        write_comment("finalize push parameters");
    }

    begin_new_line();
    m_output << "addi $sp, $sp, -1"; //update sp
    write_comment("save return address to stack");
    begin_new_line();
    m_output << "sw $ra, 0($sp)"; //save return address to stack

    // use $at as scratchpad
    std::visit(overloaded{
        [this](const std::string& function_name) -> void {
            begin_new_line();
            m_output << "la $at, " << function_name;
            begin_new_line();
            m_output << "jalr $at, $ra";
        },
        [this](const linear::virtual_register& function_vreg) -> void {
            begin_new_line();
            auto physical_function_vreg = get_physical_register(function_vreg);
            m_output << "jalr " << physical_function_vreg.name << ", $ra";
        }
    }, instruction.callee());

    // control has now been returned to the caller
    begin_new_line();
    m_output << "lw $ra, 0($sp)";
    write_comment("restore return address from stack");
    begin_new_line();
    m_output << "addi $sp, $sp, 1";

    // tear down parameters
    begin_new_line();
    m_output << "addi $sp, $sp, " << function_call_info.pushed_parameter_size;
    write_comment("tear down parameters");

    // restore caller saved registers
    for (auto [physical_register_id, offset] : function_call_info.caller_saved_registers_offsets) {
        auto& reg_info = m_current_unit.value()->platform_info.get_register_info(physical_register_id);
        begin_new_line();
        m_output << "lw " << reg_info.name << ", " << offset << "($sp)";
    }
    begin_new_line();
    m_output << "addi $sp, $sp, " << function_call_info.pushed_register_size;

    // copy return value from the return register to the destination vreg if they differ
    if (instruction.destination().has_value()) {
        auto dest_physical = get_physical_register(instruction.destination().value());
        auto return_reg_id = m_current_unit.value()->platform_info.get_return_register_id(
            instruction.destination().value().reg_class,
            instruction.destination().value().reg_size
        );
        if (dest_physical.id != return_reg_id) {
            auto& return_reg_info = m_current_unit.value()->platform_info.get_register_info(return_reg_id);
            begin_new_line();
            m_output << "add " << dest_physical.name << ", " << return_reg_info.name << ", $zero";
        }
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::function_return& instruction) {
    // pop all locals and valloca'd stuff via setting sp to fp
    begin_new_line();
    m_output << "add $sp, $fp, $zero";

    // restore callee-saved registers (saved at fp-1, fp-2, ... by prologue)
    size_t i = 0;
    for (auto& register_info : m_current_unit.value()->platform_info.registers) {
        if (register_info.is_callee_saved && !register_info.is_protected) {
            i++;
            begin_new_line();
            m_output << "lw " << register_info.name << ", -" << i << "($fp)";
        }
    }

    // restore previous frame pointer
    begin_new_line();
    m_output << "lw $fp, 0($sp)";
    begin_new_line();
    m_output << "addi $sp, $sp, 1";

    // jump to caller control
    begin_new_line();
    m_output << "jalr $ra, $zero";
}

// argument registers are $a0, $a1, $a2
static michaelcc::linear::register_t argument_registers[] = {
    3, 4, 5
};

void michaelcc::isa::lc2200::lc2200_isa::assign_parameter_registers(std::vector<linear::function_parameter>& parameters) {
    size_t offset = 0;
    size_t used_argument_registers = 0;

    // compute parameter offsets as if they were argument offsets
    std::vector<std::pair<size_t, size_t>> parameter_offsets;
    for (size_t i = 0; i < parameters.size(); i++) {
        if (parameters[i].register_class.has_value() && used_argument_registers < (sizeof(argument_registers) / sizeof(linear::register_t))) {
            if (parameters[i].register_class.value() != linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER) {
                throw std::runtime_error("Only integer registers are supported for argument passing.");
            }

            parameters[i].pass_via_register = argument_registers[used_argument_registers];
            used_argument_registers++;
        } else {
            // remeber we refer to frame pointer MINUS offset
            offset += parameters[i].layout.size;
            size_t padding = (parameters[i].layout.alignment - (offset % parameters[i].layout.alignment)) % parameters[i].layout.alignment;
            offset += padding;

            assert(padding == 0); //padding should be 0 because we align to the word size

            parameter_offsets.push_back(std::make_pair(i, offset));
        }
    }

    // look at the offsets from "the end perspective"
    for (size_t i = parameter_offsets.size(); i > 0; i--) {
        auto [parameter_index, parameter_offset] = parameter_offsets[i - 1];
        parameters[parameter_index].offset = (offset - parameter_offset) + fp_to_parameter_offset;

        // the plus two is to account for: sizeof(prev frame ptr) + sizeof(prev return addr) + sizeof(return value space)
    }
}

void michaelcc::isa::lc2200::lc2200_isa::assign_argument_registers(std::vector<linear::function_argument>& arguments) {
    size_t offset = 0;
    size_t used_argument_registers = 0;
    for (size_t i = 0; i < arguments.size(); i++) {
        if (arguments[i].register_class.has_value() && used_argument_registers < (sizeof(argument_registers) / sizeof(linear::register_t))) {
            if (arguments[i].register_class.value() != linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER) {
                throw std::runtime_error("Only integer registers are supported for argument passing.");
            }

            arguments[i].pass_via_register = argument_registers[used_argument_registers];
            used_argument_registers++;
        } else {
            // remeber we refer to frame pointer MINUS offset
            offset += arguments[i].layout.size;
            size_t padding = (arguments[i].layout.alignment - (offset % arguments[i].layout.alignment)) % arguments[i].layout.alignment;
            offset += padding;

            assert(padding == 0); //padding should be 0 because we align to the word size

            arguments[i].offset = std::make_optional(offset);
        }
    }
}