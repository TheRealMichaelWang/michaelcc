#include "linear/pass.hpp"
#include "linear/dominators.hpp"
#include "linear/allocators/remove_phi.hpp"
#include <algorithm>

#include "linear/allocators/register_allocator.hpp"
#include "linear/allocators/register_spiller.hpp"
#include "linear/allocators/frame_allocator.hpp"

namespace michaelcc::linear {
    bool transform(translation_unit& unit, std::vector<std::unique_ptr<pass>>& passes, int max_passes) {
        bool any_pass_mutated = false;
        int passes_run = 0;
        do {
            if (passes_run >= max_passes) {
                return false;
            }

            any_pass_mutated = false;
            for (auto& pass : passes) {
                pass->prescan(unit);

                bool mutated_stuff = pass->optimize(unit);
                if (mutated_stuff) {
                    compute_dominators(unit);
                }
                any_pass_mutated |= mutated_stuff;


                pass->reset();
            }
            passes_run++;
        } while (any_pass_mutated);

        return true;
    }

    void register_allocation(translation_unit& unit, allocators::frame_allocator& frame_allocator) {
        for (;;) {
            allocators::register_allocator register_allocator(unit);

            auto spilled_vregs = register_allocator.allocate();
            if (spilled_vregs.empty()) {
                break;
            }

            allocators::register_spiller register_spiller(unit, spilled_vregs);
            register_spiller.spill();

            frame_allocator.allocate();
        }
    }
}

namespace michaelcc::linear::allocators {

    struct phi_copy {
        virtual_register dest;
        virtual_register source;
    };

    // Reorder parallel copies so they can execute sequentially without
    // clobbering a source that another copy still needs. Cycles (e.g. a swap)
    // are broken by introducing a temporary vreg.
    static std::vector<phi_copy> sequentialize_parallel_copies(
        std::vector<phi_copy> copies,
        linear::translation_unit& unit
    ) {
        std::vector<phi_copy> result;

        while (!copies.empty()) {
            // Find a copy whose dest is not read by any other remaining copy
            auto safe = std::find_if(copies.begin(), copies.end(), [&](const phi_copy& c) {
                return std::none_of(copies.begin(), copies.end(), [&](const phi_copy& other) {
                    return &other != &c && other.source == c.dest;
                });
            });

            if (safe != copies.end()) {
                result.push_back(*safe);
                copies.erase(safe);
            } else {
                // All remaining copies form cycles -- break one with a temp
                auto& first = copies.front();
                auto temp = unit.new_vreg(first.source.reg_size, first.source.reg_class);
                result.push_back({ temp, first.source });
                first.source = temp;
            }
        }

        return result;
    }

    void remove_phi_nodes(linear::translation_unit& unit) {
        // Phase 1: strip phis and collect copies grouped by predecessor block
        std::unordered_map<size_t, std::vector<phi_copy>> pending_copies;

        for (auto& [block_id, block] : unit.blocks) {
            auto released_instructions = block.release_instructions();
            std::vector<std::unique_ptr<instruction>> new_instructions;
            new_instructions.reserve(released_instructions.size());

            for (auto& instruction : released_instructions) {
                if (auto phi = dynamic_cast<const phi_instruction*>(instruction.get())) {
                    for (const auto& value : phi->values()) {
                        if (phi->destination() == value.vreg) { continue; }
                        
                        pending_copies[value.block_id].push_back({
                            phi->destination(), value.vreg
                        });
                    }
                } else {
                    new_instructions.emplace_back(std::move(instruction));
                }
            }
            block.replace_instructions(std::move(new_instructions));
        }

        // Phase 2: sequentialize each predecessor's copies and insert before terminator
        for (auto& [pred_block_id, copies] : pending_copies) {
            auto sequentialized = sequentialize_parallel_copies(
                std::move(copies), unit
            );
            auto& pred_block = unit.blocks.at(pred_block_id);
            for (auto& copy : sequentialized) {
                pred_block.phi_add_instruction(std::make_unique<linear::c_instruction>(
                    linear::MICHAELCC_LINEAR_C_COPY_INIT,
                    copy.dest,
                    copy.source
                ));
            }
        }
    }
}
