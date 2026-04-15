#include "isa/lc2200.hpp"
#include "linear/ir.hpp"
#include "linear/registers.hpp"
#include "utils.hpp"
#include <vector>
#include <variant>

// how much to subtract from the frame pointer to get to the last parameter
const size_t fp_to_parameter_offset = 2;

void michaelcc::isa::lc2200::lc2200_assembler::begin_function_preamble(const linear::function_definition& definition) {
    //push old fp to the stack
    begin_new_line();
    m_output << "sw $fp, -1($sp)";

    // set fp to current sp
    begin_new_line();
    m_output << "addi $fp, $sp, 0";

    // save s0 to s2
    for (size_t i = 0; i < 3; i++) {
        begin_new_line();
        m_output << "sw $s" << i << ", -" << (i + 1) << "($sp)";
    }
    begin_new_line(); //update sp
    m_output << "add $sp, $sp, -3";

    
    //reserve space for locals
    auto& frame_allocator = *m_current_frame_allocator.value();
    size_t reserved_stack_space = frame_allocator.get_reserved_stack_space(definition.entry_block_id());    
    
    begin_new_line();
    m_output << "add $sp, $sp, -" << reserved_stack_space;
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
        begin_new_line();
        m_output << "sw " << physical_registers_to_save[i] << ", -" << (i + 1) << "($sp)";

        size_t sp_subtract_offset = physical_registers_to_save.size() - i;
        caller_saved_registers_offsets.insert({ physical_registers_to_save[i], sp_subtract_offset });
    }
    begin_new_line();
    m_output << "add $sp, $sp, -" << physical_registers_to_save.size();

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

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::a_instruction& instruction) {
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
    m_output << "lw " << physical_destination.name << ", " << instruction.offset();
    m_output << "(" << physical_source_address.name << ");";
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::store_memory& instruction) {
    auto physical_value = get_physical_register(instruction.value());
    auto physical_destination_address = get_physical_register(instruction.destination_address());

    begin_new_line();
    m_output << "sw " << physical_value.name << ", " << instruction.offset();
    m_output << "(" << physical_destination_address.name << ");";
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
        m_output << "add " << physical_destination.name << ", $fp, -" << (instruction.parameter().offset.value() + fp_to_parameter_offset);
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

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::push_function_argument& instruction) {
    auto function_call_info = m_function_call_infos.at(instruction.function_call_id());
    auto argument_physical_register = get_physical_register(instruction.value());

    if (instruction.argument().pass_via_stack()) { //save argument onto stack
        if (instruction.argument().register_class.has_value()) { //this is a register on the stack
            m_output << "sw " << argument_physical_register.name << ", -" << (instruction.argument().offset.value() + 1) << "($sp)";
        } else {
            //manual memcopy to stack
            for (size_t i = 0; i < instruction.argument().layout.size; i++) {
                // v0 is not currently in use and is therefore protected
                begin_new_line();
                m_output << "lw " << "v0, " << i << '(' << argument_physical_register.name << ')';
                begin_new_line();
                m_output << "sw " << "v0, -" << ((instruction.argument().offset.value() + 1) - i) << "($sp)";
            }
        }
        function_call_info.pushed_parameter_size = std::max(function_call_info.pushed_parameter_size, instruction.argument().offset.value() + 1);
    } else {
        // read from arg dest register into assigned a register
        if (function_call_info.trashed_registers.contains(argument_physical_register.id)) {
            // good thing v0 isn't used and is protected
            begin_new_line();
            m_output << "lw " << "v0, " << function_call_info.caller_saved_registers_offsets.at(argument_physical_register.id) << "($sp)";
            begin_new_line();
            m_output << "add " << instruction.argument().pass_via_register.value() << ", $zero, v0";
        } else {
            // read from arg dest register into assigned a register
            begin_new_line();
            m_output << "add " << instruction.argument().pass_via_register.value() << ", " << argument_physical_register.name << ", $zero";
            function_call_info.trashed_registers.insert(instruction.argument().pass_via_register.value());
        }
    }
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::function_call& instruction) {
    auto function_call_info = m_function_call_infos.at(instruction.function_call_id());

    begin_new_line();
    if (function_call_info.pushed_parameter_size > 0) { //finalize push parameters
        m_output << "addi $sp, $sp, -" << function_call_info.pushed_parameter_size;
    }

    begin_new_line();
    m_output << "addi $sp, $sp, -1"; //update sp
    begin_new_line();
    m_output << "sw $ra, ($sp)"; //save return address to stack

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
    begin_new_line();
    m_output << "addi $sp, $sp, 1";

    // tear down parameters
    begin_new_line();
    m_output << "addi $sp, $sp, " << function_call_info.pushed_parameter_size;

    // restore caller saved registers
    for (auto [physical_register_id, offset] : function_call_info.caller_saved_registers_offsets) {
        begin_new_line();
        m_output << "lw " << physical_register_id << ", " << offset << "($sp)";
    }
    begin_new_line();
    m_output << "addi $sp, $sp, " << function_call_info.pushed_register_size;
}

void michaelcc::isa::lc2200::lc2200_assembler::dispatch(const linear::function_return& instruction) {
    // assume return value has already been loaded into the destination register v0

    // pop all locals and valloca'd stuff via setting sp to fp
    begin_new_line();
    m_output << "add $sp, $fp, $zero";

    // restore previous frame pointer
    begin_new_line();
    m_output << "lw $fp, 0($sp)";
    begin_new_line();
    m_output << "add $sp, $sp, 1";

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
            size_t padding = parameters[i].layout.alignment - (offset % parameters[i].layout.alignment);
            offset += padding;

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
            size_t padding = arguments[i].layout.alignment - (offset % arguments[i].layout.alignment);
            offset += padding;

            assert(padding == 0); //padding should be 0 because we align to the word size

            arguments[i].offset = std::make_optional(offset);
        }
    }
}