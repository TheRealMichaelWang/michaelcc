#include "isa/x64.hpp"
#include "linear/ir.hpp"

using namespace michaelcc::isa::x64;

void x64_isa::legalize(linear::translation_unit& unit) const {
    for (auto& [block_id, block] : unit.blocks) {
        auto released_instructions = block.release_instructions();
        std::vector<std::unique_ptr<linear::instruction>> new_instructions;
        new_instructions.reserve(released_instructions.size());
        
        auto legalize_byte_division = [&](linear::virtual_register dest, linear::virtual_register operand_a, linear::virtual_register operand_b, bool is_division) {
            auto ax_physical_register = get_platform_info().find_register("ax");
            assert(ax_physical_register.has_value());

            // make sure numerator goes in AX, sign extend it 
            auto numerator_reg = unit.new_vreg(linear::word_size::MICHAELCC_WORD_SIZE_UINT16, linear::MICHAELCC_REGISTER_CLASS_INTEGER);
            unit.vreg_colors.insert({ numerator_reg, ax_physical_register.value() });
            new_instructions.emplace_back(std::make_unique<linear::c_instruction>(linear::MICHAELCC_LINEAR_C_SEXT_OR_TRUNC, numerator_reg, operand_a));

            // modulo out is in ah quotient out is in al
            auto out_physical_register = get_platform_info().find_register(is_division ? "al" : "ah");
            assert(out_physical_register.has_value());

            // make sure output is in AL or AH
            auto quotient_reg = unit.new_vreg(linear::word_size::MICHAELCC_WORD_SIZE_BYTE, linear::MICHAELCC_REGISTER_CLASS_INTEGER);
            unit.vreg_colors.insert({ quotient_reg, out_physical_register.value() });
            new_instructions.emplace_back(std::make_unique<linear::a_instruction>(linear::MICHAELCC_LINEAR_A_UNSIGNED_DIVIDE, quotient_reg, numerator_reg, operand_b));
            
            // copy the quotient to the destination
            new_instructions.emplace_back(std::make_unique<linear::c_instruction>(linear::MICHAELCC_LINEAR_C_COPY_INIT, dest, quotient_reg));
        };

        for (auto& instruction : released_instructions) {
            if (auto* a_instruction = dynamic_cast<const linear::a_instruction*>(instruction.get())) {
                if (a_instruction->type() == linear::MICHAELCC_LINEAR_A_UNSIGNED_DIVIDE) {
                    switch (a_instruction->destination_register()->reg_size) {
                    case linear::word_size::MICHAELCC_WORD_SIZE_BYTE: 
                        legalize_byte_division(a_instruction->destination(), a_instruction->operand_a(), a_instruction->operand_b(), true); 
                        break;
                    default: throw std::runtime_error("Invalid word size");
                    }
                }
                else if (a_instruction->type() == linear::MICHAELCC_LINEAR_A_UNSIGNED_MODULO) {
                    switch (a_instruction->destination_register()->reg_size) {
                    case linear::word_size::MICHAELCC_WORD_SIZE_BYTE: 
                        legalize_byte_division(a_instruction->destination(), a_instruction->operand_a(), a_instruction->operand_b(), false); 
                        break;
                    default: throw std::runtime_error("Invalid word size");
                    }
                }
            }
            new_instructions.emplace_back(std::move(instruction));
        }
        block.replace_instructions(std::move(new_instructions));
    }
}