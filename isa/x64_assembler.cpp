#include "isa/x64.hpp"
#include "linear/registers.hpp"
#include <queue>
#include <set>

using namespace michaelcc::isa::x64;

// rdi, rsi, rdx, rcx, r8, r9
static const michaelcc::linear::register_t sysv_int_arg_regs[] = { 24, 20, 15, 10, 36, 40 };
static constexpr size_t sysv_int_arg_count = 6;

// xmm0-xmm7
static const michaelcc::linear::register_t sysv_float_arg_regs[] = { 68, 69, 70, 71, 72, 73, 74, 75 };
static constexpr size_t sysv_float_arg_count = 8;

static std::string format_mem(const std::string& base, int64_t offset) {
    if (offset == 0) return "[" + base + "]";
    if (offset > 0) return "[" + base + " + " + std::to_string(offset) + "]";
    return "[" + base + " - " + std::to_string(-offset) + "]";
}

static const char* get_jcc_mnemonic(michaelcc::linear::a_instruction_type type) {
    using namespace michaelcc::linear;
    switch (type) {
    case MICHAELCC_LINEAR_A_COMPARE_EQUAL: return "je";
    case MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL: return "jne";
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN: return "jl";
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN_OR_EQUAL: return "jle";
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN: return "jb";
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN_OR_EQUAL: return "jbe";
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN: return "jg";
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN_OR_EQUAL: return "jge";
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN: return "ja";
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN_OR_EQUAL: return "jae";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_EQUAL: return "je";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_NOT_EQUAL: return "jne";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN: return "jb";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN_OR_EQUAL: return "jbe";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN: return "ja";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN_OR_EQUAL: return "jae";
    default: return nullptr;
    }
}

static const char* get_setcc_mnemonic(michaelcc::linear::a_instruction_type type) {
    using namespace michaelcc::linear;
    switch (type) {
    case MICHAELCC_LINEAR_A_COMPARE_EQUAL: return "sete";
    case MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL: return "setne";
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN: return "setl";
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN_OR_EQUAL: return "setle";
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN: return "setb";
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN_OR_EQUAL: return "setbe";
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN: return "setg";
    case MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN_OR_EQUAL: return "setge";
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN: return "seta";
    case MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN_OR_EQUAL: return "setae";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_EQUAL: return "sete";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_NOT_EQUAL: return "setne";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN: return "setb";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN_OR_EQUAL: return "setbe";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN: return "seta";
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN_OR_EQUAL: return "setae";
    default: return nullptr;
    }
}

static bool is_float_comparison(michaelcc::linear::a_instruction_type type) {
    using namespace michaelcc::linear;
    switch (type) {
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_EQUAL:
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_NOT_EQUAL:
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN:
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN_OR_EQUAL:
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN:
    case MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN_OR_EQUAL:
        return true;
    default:
        return false;
    }
}

void x64_assembler::begin_block_preamble(const linear::basic_block& block) {
    auto it = m_block_preamble_info.find(block.id());
    if (it != m_block_preamble_info.end()) {
        for (auto& vreg : it->second.set_to_true) {
            begin_new_line();
            m_output << "mov " << get_physical_register(vreg).name << ", 1";
        }
        for (auto& vreg : it->second.set_to_false) {
            begin_new_line();
            m_output << "mov " << get_physical_register(vreg).name << ", 0";
        }
    }
}

void x64_assembler::begin_function_preamble(const linear::function_definition& definition) {
    begin_new_line();
    m_output << "push rbp";
    begin_new_line();
    m_output << "mov rbp, rsp";

    std::set<linear::register_t> callee_saved_64;
    std::unordered_set<size_t> visited;
    std::queue<size_t> worklist;
    worklist.push(definition.entry_block_id());

    while (!worklist.empty()) {
        size_t block_id = worklist.front();
        worklist.pop();
        if (visited.contains(block_id)) continue;
        visited.insert(block_id);

        auto& block = m_current_unit.value()->blocks.at(block_id);
        for (auto& instr : block.instructions()) {
            auto dest = instr->destination_register();
            if (!dest) continue;
            auto color_it = m_current_unit.value()->vreg_colors.find(*dest);
            if (color_it == m_current_unit.value()->vreg_colors.end()) continue;
            auto reg_info = m_current_unit.value()->platform_info.get_register_info(color_it->second);
            if (reg_info.is_callee_saved && !reg_info.is_protected) {
                auto reg_64 = get_physical_of_size(color_it->second, linear::MICHAELCC_WORD_SIZE_UINT64);
                callee_saved_64.insert(reg_64.id);
            }
        }
        for (size_t succ : block.successor_block_ids()) {
            worklist.push(succ);
        }
    }

    m_callee_saved_regs.clear();
    for (auto reg_id : callee_saved_64) {
        auto reg_info = m_current_unit.value()->platform_info.get_register_info(reg_id);
        begin_new_line();
        m_output << "push " << reg_info.name;
        m_callee_saved_regs.push_back(reg_id);
    }
}

void x64_assembler::begin_function_call(const linear::function_call& instruction) {
    pending_call_info info;

    for (auto& vreg : instruction.caller_saved_registers()) {
        auto phys = get_physical_register(vreg);
        if (phys.reg_class == linear::MICHAELCC_REGISTER_CLASS_INTEGER) {
            auto phys_64 = get_physical_of_size(phys.id, linear::MICHAELCC_WORD_SIZE_UINT64);
            begin_new_line();
            m_output << "push " << phys_64.name;
            info.saved_int_regs.push_back(phys_64);
        } else {
            begin_new_line();
            m_output << "sub rsp, 8";
            begin_new_line();
            m_output << "movsd [rsp], " << phys.name;
            info.saved_float_regs.push_back(phys);
        }
    }

    m_pending_calls[instruction.function_call_id()] = std::move(info);
}

void x64_assembler::emit_unsigned_multiply(linear::virtual_register, linear::virtual_register, linear::virtual_register) {
    throw std::runtime_error("unsigned multiply not implemented; needs legalization to imul two-operand form");
}

void x64_assembler::emit_signed_multiply(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto phys_dest = get_physical_register(dest);
    auto phys_a = get_physical_register(operand_a);
    auto phys_b = get_physical_register(operand_b);

    if (dest == operand_a) {
        begin_new_line();
        m_output << "imul " << phys_dest.name << ", " << phys_b.name;
    } else if (dest == operand_b) {
        begin_new_line();
        m_output << "imul " << phys_dest.name << ", " << phys_a.name;
    } else {
        begin_new_line();
        m_output << "mov " << phys_dest.name << ", " << phys_a.name;
        begin_new_line();
        m_output << "imul " << phys_dest.name << ", " << phys_b.name;
    }
}

void x64_assembler::emit_signed_divide(linear::virtual_register, linear::virtual_register, linear::virtual_register) {
    throw std::runtime_error("signed divide not implemented; needs rax/rdx pre-coloring");
}

void x64_assembler::emit_signed_remainder(linear::virtual_register, linear::virtual_register, linear::virtual_register) {
    throw std::runtime_error("signed remainder not implemented; needs rax/rdx pre-coloring");
}

void x64_assembler::emit_unsigned_divide(linear::virtual_register, linear::virtual_register, linear::virtual_register) {
    throw std::runtime_error("unsigned divide not implemented; needs rax/rdx pre-coloring");
}

void x64_assembler::emit_unsigned_remainder(linear::virtual_register, linear::virtual_register, linear::virtual_register) {
    throw std::runtime_error("unsigned remainder not implemented; needs rax/rdx pre-coloring");
}

void x64_assembler::emit_logical_and(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_reg_a = get_physical_register(operand_a);
    auto physical_reg_b = get_physical_register(operand_b);

    if (auto ni = next_instruction()) {
        if (auto branch_condition = dynamic_cast<const linear::branch_condition*>(*ni)) {
            if (branch_condition->condition() == dest) {
                begin_new_line();
                m_output << "test " << physical_reg_a.name << ", " << physical_reg_a.name;
                begin_new_line();
                m_output << "jz block" << branch_condition->if_false_block_id();
                begin_new_line();
                m_output << "test " << physical_reg_b.name << ", " << physical_reg_b.name;
                begin_new_line();
                m_output << "jz block" << branch_condition->if_false_block_id();
                begin_new_line();
                m_output << "jmp block" << branch_condition->if_true_block_id();

                skip_next_instruction();
                add_set_true_to_block_preamble(branch_condition->if_true_block_id(), dest);
                add_set_false_to_block_preamble(branch_condition->if_false_block_id(), dest);
                return;
            }
        }
    }

    throw std::runtime_error("logical AND as a value not supported yet");
}

void x64_assembler::emit_logical_or(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_reg_a = get_physical_register(operand_a);
    auto physical_reg_b = get_physical_register(operand_b);

    if (auto ni = next_instruction()) {
        if (auto branch_condition = dynamic_cast<const linear::branch_condition*>(*ni)) {
            if (branch_condition->condition() == dest) {
                begin_new_line();
                m_output << "test " << physical_reg_a.name << ", " << physical_reg_a.name;
                begin_new_line();
                m_output << "jnz block" << branch_condition->if_true_block_id();
                begin_new_line();
                m_output << "test " << physical_reg_b.name << ", " << physical_reg_b.name;
                begin_new_line();
                m_output << "jnz block" << branch_condition->if_true_block_id();
                begin_new_line();
                m_output << "jmp block" << branch_condition->if_false_block_id();

                skip_next_instruction();
                add_set_true_to_block_preamble(branch_condition->if_true_block_id(), dest);
                add_set_false_to_block_preamble(branch_condition->if_false_block_id(), dest);
                return;
            }
        }
    }

    throw std::runtime_error("logical OR as a value not supported yet");
}

void x64_assembler::emit_logical_xor(linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b) {
    auto physical_reg_a = get_physical_register(operand_a);
    auto physical_reg_b = get_physical_register(operand_b);

    if (auto ni = next_instruction()) {
        if (auto branch_condition = dynamic_cast<const linear::branch_condition*>(*ni)) {
            if (branch_condition->condition() == dest) {
                begin_new_line();
                m_output << "test " << physical_reg_a.name << ", " << physical_reg_a.name;

                std::string a_false_label = generate_symbol();
                begin_new_line();
                m_output << "jz " << a_false_label;

                begin_new_line();
                m_output << "test " << physical_reg_b.name << ", " << physical_reg_b.name;
                begin_new_line();
                m_output << "jz block" << branch_condition->if_true_block_id();
                begin_new_line();
                m_output << "jmp block" << branch_condition->if_false_block_id();

                emit_label(a_false_label);
                begin_new_line();
                m_output << "test " << physical_reg_b.name << ", " << physical_reg_b.name;
                begin_new_line();
                m_output << "jnz block" << branch_condition->if_true_block_id();
                begin_new_line();
                m_output << "jmp block" << branch_condition->if_false_block_id();

                skip_next_instruction();
                add_set_true_to_block_preamble(branch_condition->if_true_block_id(), dest);
                add_set_false_to_block_preamble(branch_condition->if_false_block_id(), dest);
                return;
            }
        }
    }

    throw std::runtime_error("logical XOR as a value not supported yet");
}

void x64_assembler::dispatch(const linear::a_instruction& instruction) {
    switch (instruction.type()) {
    case linear::MICHAELCC_LINEAR_A_SIGNED_MULTIPLY:
        emit_signed_multiply(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY:
        emit_unsigned_multiply(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_SIGNED_DIVIDE:
        emit_signed_divide(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_UNSIGNED_DIVIDE:
        emit_unsigned_divide(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_SIGNED_MODULO:
        emit_signed_remainder(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_UNSIGNED_MODULO:
        emit_unsigned_remainder(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_AND:
        emit_logical_and(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_OR:
        emit_logical_or(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_XOR:
        emit_logical_xor(instruction.destination(), instruction.operand_a(), instruction.operand_b());
        return;
    case linear::MICHAELCC_LINEAR_A_SHIFT_LEFT:
    case linear::MICHAELCC_LINEAR_A_SIGNED_SHIFT_RIGHT:
    case linear::MICHAELCC_LINEAR_A_UNSIGNED_SHIFT_RIGHT:
        throw std::runtime_error("register-register shifts not implemented; use a2_instruction");

    case linear::MICHAELCC_LINEAR_A_COMPARE_EQUAL:
    case linear::MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL:
    case linear::MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN:
    case linear::MICHAELCC_LINEAR_A_COMPARE_SIGNED_LESS_THAN_OR_EQUAL:
    case linear::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN:
    case linear::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_LESS_THAN_OR_EQUAL:
    case linear::MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN:
    case linear::MICHAELCC_LINEAR_A_COMPARE_SIGNED_GREATER_THAN_OR_EQUAL:
    case linear::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN:
    case linear::MICHAELCC_LINEAR_A_COMPARE_UNSIGNED_GREATER_THAN_OR_EQUAL:
    case linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_EQUAL:
    case linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_NOT_EQUAL:
    case linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN:
    case linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_LESS_THAN_OR_EQUAL:
    case linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN:
    case linear::MICHAELCC_LINEAR_A_FLOAT_COMPARE_GREATER_THAN_OR_EQUAL:
    {
        auto phys_a = get_physical_register(instruction.operand_a());
        auto phys_b = get_physical_register(instruction.operand_b());

        begin_new_line();
        if (is_float_comparison(instruction.type())) {
            if (instruction.operand_a().reg_size == linear::MICHAELCC_WORD_SIZE_UINT64) {
                m_output << "ucomisd " << phys_a.name << ", " << phys_b.name;
            } else {
                m_output << "ucomiss " << phys_a.name << ", " << phys_b.name;
            }
        } else {
            m_output << "cmp " << phys_a.name << ", " << phys_b.name;
        }

        if (auto ni = next_instruction()) {
            if (auto* bc = dynamic_cast<const linear::branch_condition*>(*ni)) {
                if (bc->condition() == instruction.destination()) {
                    begin_new_line();
                    m_output << get_jcc_mnemonic(instruction.type()) << " block" << bc->if_true_block_id();
                    begin_new_line();
                    m_output << "jmp block" << bc->if_false_block_id();
                    skip_next_instruction();
                    return;
                }
            }
        }

        auto phys_dest = get_physical_register(instruction.destination());
        auto byte_dest = get_physical_of_size(
            m_current_unit.value()->vreg_colors.at(instruction.destination()),
            linear::MICHAELCC_WORD_SIZE_BYTE
        );
        begin_new_line();
        m_output << get_setcc_mnemonic(instruction.type()) << " " << byte_dest.name;
        if (instruction.destination().reg_size != linear::MICHAELCC_WORD_SIZE_BYTE) {
            begin_new_line();
            m_output << "movzx " << phys_dest.name << ", " << byte_dest.name;
        }
        return;
    }

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
            if (instruction.operand_a() != instruction.destination()) {
                m_output << "movsd " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
            }
            m_output << "divsd " << physical_reg_dest.name << ", " << physical_reg_b.name;
        } else {
            if (instruction.operand_a() != instruction.destination()) {
                m_output << "movss " << physical_reg_dest.name << ", " << physical_reg_a.name;
                begin_new_line();
            }
            m_output << "divss " << physical_reg_dest.name << ", " << physical_reg_b.name;
        }
        break;
    case linear::MICHAELCC_LINEAR_A_FLOAT_MODULO:
        throw std::runtime_error("float modulo not supported; use fmod");
    
    default: throw std::runtime_error("unhandled a_instruction type");
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
    default: throw std::runtime_error("unhandled a2_instruction type");
    }
}

void x64_assembler::dispatch(const linear::u_instruction& instruction) {
    auto phys_dest = get_physical_register(instruction.destination());
    auto phys_src = get_physical_register(instruction.operand());

    switch (instruction.type()) {
    case linear::MICHAELCC_LINEAR_U_NEGATE:
        if (phys_dest.id != phys_src.id) {
            begin_new_line();
            m_output << "mov " << phys_dest.name << ", " << phys_src.name;
        }
        begin_new_line();
        m_output << "neg " << phys_dest.name;
        break;

    case linear::MICHAELCC_LINEAR_U_FLOAT_NEGATE:
        if (phys_dest.id == phys_src.id)
            throw std::runtime_error("float negate with dest == src not supported; needs scratch register");
        begin_new_line();
        m_output << "pxor " << phys_dest.name << ", " << phys_dest.name;
        begin_new_line();
        if (instruction.operand().reg_size == linear::MICHAELCC_WORD_SIZE_UINT64)
            m_output << "subsd " << phys_dest.name << ", " << phys_src.name;
        else
            m_output << "subss " << phys_dest.name << ", " << phys_src.name;
        break;

    case linear::MICHAELCC_LINEAR_U_NOT:
    {
        begin_new_line();
        m_output << "test " << phys_src.name << ", " << phys_src.name;
        auto byte_dest = get_physical_of_size(
            m_current_unit.value()->vreg_colors.at(instruction.destination()),
            linear::MICHAELCC_WORD_SIZE_BYTE
        );
        begin_new_line();
        m_output << "sete " << byte_dest.name;
        if (instruction.destination().reg_size != linear::MICHAELCC_WORD_SIZE_BYTE) {
            begin_new_line();
            m_output << "movzx " << phys_dest.name << ", " << byte_dest.name;
        }
        break;
    }

    case linear::MICHAELCC_LINEAR_U_BITWISE_NOT:
        if (phys_dest.id != phys_src.id) {
            begin_new_line();
            m_output << "mov " << phys_dest.name << ", " << phys_src.name;
        }
        begin_new_line();
        m_output << "not " << phys_dest.name;
        break;

    default:
        throw std::runtime_error("unhandled u_instruction type");
    }
}

void x64_assembler::dispatch(const linear::c_instruction& instruction) {
    auto phys_dest = get_physical_register(instruction.destination());
    auto phys_src = get_physical_register(instruction.source());

    switch (instruction.type()) {
    case linear::MICHAELCC_LINEAR_C_COPY_INIT:
        if (phys_dest.id == phys_src.id) break;
        begin_new_line();
        if (instruction.destination().reg_class == linear::MICHAELCC_REGISTER_CLASS_FLOATING_POINT) {
            if (instruction.destination().reg_size == linear::MICHAELCC_WORD_SIZE_UINT64)
                m_output << "movsd " << phys_dest.name << ", " << phys_src.name;
            else
                m_output << "movss " << phys_dest.name << ", " << phys_src.name;
        } else {
            m_output << "mov " << phys_dest.name << ", " << phys_src.name;
        }
        break;

    case linear::MICHAELCC_LINEAR_C_SEXT_OR_TRUNC:
        if (instruction.is_extension()) {
            begin_new_line();
            if (instruction.source().reg_size == linear::MICHAELCC_WORD_SIZE_UINT32)
                m_output << "movsxd " << phys_dest.name << ", " << phys_src.name;
            else
                m_output << "movsx " << phys_dest.name << ", " << phys_src.name;
        } else {
            auto src_at_dest_size = get_physical_of_size(phys_src.id, instruction.destination().reg_size);
            if (phys_dest.id != src_at_dest_size.id) {
                begin_new_line();
                m_output << "mov " << phys_dest.name << ", " << src_at_dest_size.name;
            }
        }
        break;

    case linear::MICHAELCC_LINEAR_C_ZEXT_OR_TRUNC:
        if (instruction.is_extension()) {
            begin_new_line();
            if (instruction.source().reg_size == linear::MICHAELCC_WORD_SIZE_UINT32) {
                auto dest_32 = get_physical_of_size(phys_dest.id, linear::MICHAELCC_WORD_SIZE_UINT32);
                m_output << "mov " << dest_32.name << ", " << phys_src.name;
            } else {
                m_output << "movzx " << phys_dest.name << ", " << phys_src.name;
            }
        } else {
            auto src_at_dest_size = get_physical_of_size(phys_src.id, instruction.destination().reg_size);
            if (phys_dest.id != src_at_dest_size.id) {
                begin_new_line();
                m_output << "mov " << phys_dest.name << ", " << src_at_dest_size.name;
            }
        }
        break;

    case linear::MICHAELCC_LINEAR_C_FLOAT64_TO_SIGNED_INT64:
        begin_new_line();
        m_output << "cvttsd2si " << phys_dest.name << ", " << phys_src.name;
        break;
    case linear::MICHAELCC_LINEAR_C_FLOAT32_TO_SIGNED_INT32:
        begin_new_line();
        m_output << "cvttss2si " << phys_dest.name << ", " << phys_src.name;
        break;
    case linear::MICHAELCC_LINEAR_C_SIGNED_INT64_TO_FLOAT64:
        begin_new_line();
        m_output << "cvtsi2sd " << phys_dest.name << ", " << phys_src.name;
        break;
    case linear::MICHAELCC_LINEAR_C_SIGNED_INT32_TO_FLOAT32:
        begin_new_line();
        m_output << "cvtsi2ss " << phys_dest.name << ", " << phys_src.name;
        break;
    case linear::MICHAELCC_LINEAR_C_FLOAT32_TO_FLOAT64:
        begin_new_line();
        m_output << "cvtss2sd " << phys_dest.name << ", " << phys_src.name;
        break;
    case linear::MICHAELCC_LINEAR_C_FLOAT64_TO_FLOAT32:
        begin_new_line();
        m_output << "cvtsd2ss " << phys_dest.name << ", " << phys_src.name;
        break;

    case linear::MICHAELCC_LINEAR_C_FLOAT64_TO_UNSIGNED_INT64:
    case linear::MICHAELCC_LINEAR_C_FLOAT32_TO_UNSIGNED_INT32:
    case linear::MICHAELCC_LINEAR_C_UNSIGNED_INT64_TO_FLOAT64:
    case linear::MICHAELCC_LINEAR_C_UNSIGNED_INT32_TO_FLOAT32:
        throw std::runtime_error("unsigned float/int conversions not implemented");

    default:
        throw std::runtime_error("unhandled c_instruction type");
    }
}

void x64_assembler::dispatch(const linear::init_register& instruction) {
    auto phys = get_physical_register(instruction.destination());

    if (instruction.destination().reg_class == linear::MICHAELCC_REGISTER_CLASS_FLOATING_POINT) {
        if (instruction.value().uint64 == 0) {
            begin_new_line();
            m_output << "pxor " << phys.name << ", " << phys.name;
        } else {
            throw std::runtime_error("non-zero float immediate not supported; use data section + load");
        }
        return;
    }

    begin_new_line();
    switch (instruction.destination().reg_size) {
    case linear::MICHAELCC_WORD_SIZE_BYTE:
        m_output << "mov " << phys.name << ", " << static_cast<unsigned>(instruction.value().ubyte);
        break;
    case linear::MICHAELCC_WORD_SIZE_UINT16:
        m_output << "mov " << phys.name << ", " << instruction.value().uint16;
        break;
    case linear::MICHAELCC_WORD_SIZE_UINT32:
        m_output << "mov " << phys.name << ", " << instruction.value().uint32;
        break;
    case linear::MICHAELCC_WORD_SIZE_UINT64:
        m_output << "mov " << phys.name << ", " << instruction.value().uint64;
        break;
    }
}

void x64_assembler::dispatch(const linear::load_parameter& instruction) {
    auto phys_dest = get_physical_register(instruction.destination());

    if (instruction.is_address())
        throw std::runtime_error("stack-passed parameters not implemented");

    linear::register_info abi_reg{};
    if (instruction.parameter().register_class == linear::MICHAELCC_REGISTER_CLASS_INTEGER) {
        if (instruction.parameter().index >= sysv_int_arg_count)
            throw std::runtime_error("too many integer parameters for SysV register passing");
        abi_reg = get_physical_of_size(sysv_int_arg_regs[instruction.parameter().index], instruction.destination().reg_size);
    } else {
        if (instruction.parameter().index >= sysv_float_arg_count)
            throw std::runtime_error("too many float parameters for SysV register passing");
        abi_reg = m_current_unit.value()->platform_info.get_register_info(sysv_float_arg_regs[instruction.parameter().index]);
    }

    if (phys_dest.id == abi_reg.id) return;

    begin_new_line();
    if (instruction.parameter().register_class == linear::MICHAELCC_REGISTER_CLASS_FLOATING_POINT) {
        if (instruction.destination().reg_size == linear::MICHAELCC_WORD_SIZE_UINT64)
            m_output << "movsd " << phys_dest.name << ", " << abi_reg.name;
        else
            m_output << "movss " << phys_dest.name << ", " << abi_reg.name;
    } else {
        m_output << "mov " << phys_dest.name << ", " << abi_reg.name;
    }
}

void x64_assembler::dispatch(const linear::load_effective_address& instruction) {
    auto phys_dest = get_physical_register(instruction.destination());
    begin_new_line();
    m_output << "lea " << phys_dest.name << ", [" << instruction.label() << "]";
}

void x64_assembler::dispatch(const linear::load_memory& instruction) {
    auto phys_dest = get_physical_register(instruction.destination());
    auto phys_addr = get_physical_register(instruction.source_address());
    auto mem = format_mem(phys_addr.name, instruction.offset());

    begin_new_line();
    if (instruction.destination().reg_class == linear::MICHAELCC_REGISTER_CLASS_FLOATING_POINT) {
        if (instruction.size_to_read() == linear::MICHAELCC_WORD_SIZE_UINT64)
            m_output << "movsd " << phys_dest.name << ", " << mem;
        else
            m_output << "movss " << phys_dest.name << ", " << mem;
    } else {
        m_output << "mov " << phys_dest.name << ", " << mem;
    }
}

void x64_assembler::dispatch(const linear::store_memory& instruction) {
    auto phys_addr = get_physical_register(instruction.source_address());
    auto phys_val = get_physical_register(instruction.value());
    auto mem = format_mem(phys_addr.name, instruction.offset());

    begin_new_line();
    if (instruction.value().reg_class == linear::MICHAELCC_REGISTER_CLASS_FLOATING_POINT) {
        if (instruction.size_to_write() == linear::MICHAELCC_WORD_SIZE_UINT64)
            m_output << "movsd " << mem << ", " << phys_val.name;
        else
            m_output << "movss " << mem << ", " << phys_val.name;
    } else {
        m_output << "mov " << mem << ", " << phys_val.name;
    }
}

void x64_assembler::dispatch(const linear::alloca_instruction& instruction) {
    auto phys_dest = get_physical_register(instruction.destination());
    begin_new_line();
    m_output << "sub rsp, " << instruction.size_bytes();
    if (instruction.alignment() > 1) {
        begin_new_line();
        m_output << "and rsp, -" << instruction.alignment();
    }
    begin_new_line();
    m_output << "mov " << phys_dest.name << ", rsp";
}

void x64_assembler::dispatch(const linear::valloca_instruction& instruction) {
    auto phys_dest = get_physical_register(instruction.destination());
    auto phys_size = get_physical_register(instruction.size());
    auto size_64 = get_physical_of_size(phys_size.id, linear::MICHAELCC_WORD_SIZE_UINT64);
    begin_new_line();
    m_output << "sub rsp, " << size_64.name;
    if (instruction.alignment() > 1) {
        begin_new_line();
        m_output << "and rsp, -" << instruction.alignment();
    }
    begin_new_line();
    m_output << "mov " << phys_dest.name << ", rsp";
}

void x64_assembler::dispatch(const linear::branch& instruction) {
    begin_new_line();
    m_output << "jmp block" << instruction.next_block_id();
}

void x64_assembler::dispatch(const linear::branch_condition& instruction) {
    auto phys_cond = get_physical_register(instruction.condition());
    begin_new_line();
    m_output << "test " << phys_cond.name << ", " << phys_cond.name;
    begin_new_line();
    m_output << "jnz block" << instruction.if_true_block_id();
    begin_new_line();
    m_output << "jmp block" << instruction.if_false_block_id();
}

void x64_assembler::dispatch(const linear::push_function_argument& instruction) {
    auto phys_val = get_physical_register(instruction.value());

    if (instruction.is_address()) {
        auto& info = m_pending_calls[instruction.function_call_id()];
        begin_new_line();
        if (instruction.value().reg_class == linear::MICHAELCC_REGISTER_CLASS_FLOATING_POINT) {
            m_output << "sub rsp, 8";
            begin_new_line();
            if (instruction.value().reg_size == linear::MICHAELCC_WORD_SIZE_UINT64)
                m_output << "movsd [rsp], " << phys_val.name;
            else
                m_output << "movss [rsp], " << phys_val.name;
        } else {
            auto val_64 = get_physical_of_size(phys_val.id, linear::MICHAELCC_WORD_SIZE_UINT64);
            m_output << "push " << val_64.name;
        }
        info.num_stack_arg_bytes += 8;
        return;
    }

    linear::register_info abi_reg{};
    if (instruction.argument().register_class == linear::MICHAELCC_REGISTER_CLASS_INTEGER) {
        if (instruction.argument().index >= sysv_int_arg_count)
            throw std::runtime_error("too many integer arguments for SysV register passing");
        abi_reg = get_physical_of_size(sysv_int_arg_regs[instruction.argument().index], instruction.value().reg_size);
    } else {
        if (instruction.argument().index >= sysv_float_arg_count)
            throw std::runtime_error("too many float arguments for SysV register passing");
        abi_reg = m_current_unit.value()->platform_info.get_register_info(sysv_float_arg_regs[instruction.argument().index]);
    }

    if (phys_val.id == abi_reg.id) return;

    begin_new_line();
    if (instruction.argument().register_class == linear::MICHAELCC_REGISTER_CLASS_FLOATING_POINT) {
        if (instruction.value().reg_size == linear::MICHAELCC_WORD_SIZE_UINT64)
            m_output << "movsd " << abi_reg.name << ", " << phys_val.name;
        else
            m_output << "movss " << abi_reg.name << ", " << phys_val.name;
    } else {
        m_output << "mov " << abi_reg.name << ", " << phys_val.name;
    }
}

void x64_assembler::dispatch(const linear::function_call& instruction) {
    begin_new_line();
    std::visit(michaelcc::overloaded{
        [this](const std::string& label) { m_output << "call " << label; },
        [this](const linear::virtual_register& vreg) { m_output << "call " << get_physical_register(vreg).name; }
    }, instruction.callee());

    if (instruction.destination()) {
        auto dest_vreg = *instruction.destination();
        auto phys_dest = get_physical_register(dest_vreg);
        auto return_reg_id = m_current_unit.value()->platform_info.get_return_register_id(dest_vreg.reg_class, dest_vreg.reg_size);
        auto return_reg = m_current_unit.value()->platform_info.get_register_info(return_reg_id);

        if (phys_dest.id != return_reg.id) {
            begin_new_line();
            if (dest_vreg.reg_class == linear::MICHAELCC_REGISTER_CLASS_FLOATING_POINT) {
                if (dest_vreg.reg_size == linear::MICHAELCC_WORD_SIZE_UINT64)
                    m_output << "movsd " << phys_dest.name << ", " << return_reg.name;
                else
                    m_output << "movss " << phys_dest.name << ", " << return_reg.name;
            } else {
                m_output << "mov " << phys_dest.name << ", " << return_reg.name;
            }
        }
    }

    auto it = m_pending_calls.find(instruction.function_call_id());
    if (it != m_pending_calls.end()) {
        auto& info = it->second;

        if (info.num_stack_arg_bytes > 0) {
            begin_new_line();
            m_output << "add rsp, " << info.num_stack_arg_bytes;
        }

        for (auto rit = info.saved_float_regs.rbegin(); rit != info.saved_float_regs.rend(); ++rit) {
            begin_new_line();
            m_output << "movsd " << rit->name << ", [rsp]";
            begin_new_line();
            m_output << "add rsp, 8";
        }

        for (auto rit = info.saved_int_regs.rbegin(); rit != info.saved_int_regs.rend(); ++rit) {
            begin_new_line();
            m_output << "pop " << rit->name;
        }

        m_pending_calls.erase(it);
    }
}

void x64_assembler::dispatch(const linear::function_return& instruction) {
    if (!m_callee_saved_regs.empty()) {
        begin_new_line();
        m_output << "lea rsp, [rbp - " << m_callee_saved_regs.size() * 8 << "]";
        for (auto rit = m_callee_saved_regs.rbegin(); rit != m_callee_saved_regs.rend(); ++rit) {
            begin_new_line();
            m_output << "pop " << m_current_unit.value()->platform_info.get_register_info(*rit).name;
        }
    }
    begin_new_line();
    m_output << "pop rbp";
    begin_new_line();
    m_output << "ret";
}
