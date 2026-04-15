#include "assembly/assembler.hpp"
#include "linear/allocators/frame_allocator.hpp"
#include "linear/ir.hpp"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>

void michaelcc::assembly::assembler::assemble_block(const linear::translation_unit& unit, size_t block_id, bool emit_label) {
    std::unordered_map<size_t, const linear::function_call*> function_id_to_call;

    auto& block = unit.blocks.at(block_id);
    for (const auto& instruction : block.instructions()) {
        if (auto* call = dynamic_cast<const linear::function_call*>(instruction.get())) {
            function_id_to_call.insert({ call->function_call_id(), call });
        }
    }

    if (emit_label) {
        m_output << "block" << block_id << ":";
    }
    begin_block_preamble(block);

    std::unordered_set<size_t> begun_function_calls;
    for (auto it = block.instructions().begin(); it != block.instructions().end(); it++) {
        auto& instruction = *it;

        if (m_skip_next_instruction) {
            m_skip_next_instruction = false;
            continue;
        }

        if ((it + 1) != block.instructions().end()) {
            m_next_instruction = (it + 1)->get();
        } else {
            m_next_instruction = std::nullopt;
        }

        if (auto* push_arg = dynamic_cast<const linear::push_function_argument*>(instruction.get())) {
            if (!begun_function_calls.contains(push_arg->function_call_id())) {
                auto* call = function_id_to_call.at(push_arg->function_call_id());
                m_current_unit = std::make_optional(&unit);
                begin_function_call(*call);
                begun_function_calls.insert(push_arg->function_call_id());
                m_current_unit = std::nullopt;
            }
        }

        // dispatch the instruction to individual assembler methods
        m_current_unit = std::make_optional(&unit);
        (*this)(*instruction);
        m_current_unit = std::nullopt;
        m_next_instruction = std::nullopt;
    }
}

void michaelcc::assembly::assembler::assemble_function(const linear::translation_unit& unit, size_t function_id) {
    auto& function = unit.function_definitions.at(function_id);
    std::vector<size_t> blocks_to_assemble;
    blocks_to_assemble.push_back(function->entry_block_id());

    while (!blocks_to_assemble.empty()) {
        size_t block_id = blocks_to_assemble.back();
        blocks_to_assemble.pop_back();

        if (m_assembled_blocks.contains(block_id)) {
            continue;
        }
        m_assembled_blocks.insert(block_id);

        if (block_id == function->entry_block_id()) {
            emit_label(function->name());
            m_current_unit = std::make_optional(&unit);
            begin_function_preamble(*function);
            m_current_unit = std::nullopt;
            assemble_block(unit, block_id, false);
        } else {
            assemble_block(unit, block_id, true);
        }

        auto& block = unit.blocks.at(block_id);
        for (size_t successor : block.successor_block_ids()) {
            if (std::find(prioritized_blocks_to_assemble.begin(), prioritized_blocks_to_assemble.end(), successor) != prioritized_blocks_to_assemble.end()) {
                continue;
            }
            if (!m_assembled_blocks.contains(successor)) {
                blocks_to_assemble.push_back(successor);
            }
        }
        
        for (auto it = prioritized_blocks_to_assemble.rbegin(); it != prioritized_blocks_to_assemble.rend(); it++) {
            blocks_to_assemble.push_back(*it);
        }
        prioritized_blocks_to_assemble.clear();
    }
}

void michaelcc::assembly::assembler::assemble(const linear::translation_unit& unit, const linear::allocators::frame_allocator& frame_allocator) {
    m_current_frame_allocator = std::make_optional(&frame_allocator);
    for (size_t i = 0; i < unit.function_definitions.size(); i++) {
        assemble_function(unit, i);
    }
    m_current_frame_allocator = std::nullopt;
}