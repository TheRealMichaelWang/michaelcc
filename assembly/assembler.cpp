#include "assembly/assembler.hpp"
#include "linear/ir.hpp"
#include <unordered_map>
#include <unordered_set>

void michaelcc::assembly::assembler::assemble_block(const linear::translation_unit& unit, size_t block_id) {
    std::unordered_map<size_t, const linear::function_call*> function_id_to_call;

    auto& block = unit.blocks.at(block_id);
    for (const auto& instruction : block.instructions()) {
        if (auto* call = dynamic_cast<const linear::function_call*>(instruction.get())) {
            function_id_to_call.insert({ call->function_call_id(), call });
        }
    }

    std::unordered_set<size_t> begun_function_calls;
    for (const auto& instruction : block.instructions()) {
        if (auto* push_arg = dynamic_cast<const linear::push_function_argument*>(instruction.get())) {
            if (!begun_function_calls.contains(push_arg->function_call_id())) {
                auto* call = function_id_to_call.at(push_arg->function_call_id());
                begin_function_call(*call, m_output);
                begun_function_calls.insert(push_arg->function_call_id());
            }
        }

        // dispatch the instruction to individual assembler methods
        (*this)(*instruction);
    }
}

void michaelcc::assembly::assembler::assemble(const linear::translation_unit& unit) {
    for (const auto& [block_id, _] : unit.blocks) {
        assemble_block(unit, block_id);
    }
}