#include "linear/optimization/phi.hpp"
#include "linear/allocators/register_allocator.hpp"
#include "linear/allocators/register_spiller.hpp"
#include "linear/allocators/frame_allocator.hpp"
#include "linear/allocators/remove_phi.hpp"

namespace michaelcc::linear::optimization::postphi {
    void register_allocation(translation_unit& unit, allocators::frame_allocator& frame_allocator) {
        for (;;) {
            allocators::register_allocator register_allocator(unit);

            auto spilled_vregs = register_allocator.allocate();
            eliminate_redundant_copies(unit);
            if (spilled_vregs.empty()) {
                break;
            }

            allocators::register_spiller register_spiller(unit, spilled_vregs);
            register_spiller.spill();
            frame_allocator.allocate();
            simplify_frame_arithmetic(unit);
            eliminate_redundant_copies(unit);
        }
    }

    void eliminate_redundant_copies(translation_unit& unit) {
        for (auto& [block_id, block] : unit.blocks) {
            auto released = block.release_instructions();
            std::vector<std::unique_ptr<instruction>> new_instructions;
            new_instructions.reserve(released.size());
            for (auto& inst : released) {
                if (auto* copy = dynamic_cast<const c_instruction*>(inst.get())) {
                    if (copy->type() == MICHAELCC_LINEAR_C_COPY_INIT
                        && unit.vreg_colors.contains(copy->source())
                        && unit.vreg_colors.contains(copy->destination())
                        && unit.vreg_colors.at(copy->source()) == unit.vreg_colors.at(copy->destination())) {
                        continue;
                    }
                }
                new_instructions.emplace_back(std::move(inst));
            }
            block.replace_instructions(std::move(new_instructions));
        }
    }

    void simplify_frame_arithmetic(translation_unit& unit) {
        std::unordered_map<virtual_register, a2_instruction> a2_defs;

        for (const auto& [block_id, block] : unit.blocks) {
            for (const auto& inst : block.instructions()) {
                if (auto* a2 = dynamic_cast<const a2_instruction*>(inst.get())) {
                    a2_defs.insert({ a2->destination(), *a2 });
                }
            }
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& [block_id, block] : unit.blocks) {
                auto released = block.release_instructions();
                std::vector<std::unique_ptr<instruction>> new_instructions;
                new_instructions.reserve(released.size());

                for (auto& inst : released) {
                    // fold a2(a2(base, c1), c2) → a2(base, c1 op c2)
                    if (auto* a2 = dynamic_cast<const a2_instruction*>(inst.get())) {
                        auto inner_it = a2_defs.find(a2->operand_a());
                        if (inner_it != a2_defs.end()) {
                            auto& inner = inner_it->second;
                            std::optional<std::unique_ptr<instruction>> folded;

                            if (a2->type() == inner.type()) {
                                switch (a2->type()) {
                                case MICHAELCC_LINEAR_A_ADD:
                                    folded = std::make_unique<a2_instruction>(a2->type(), a2->destination(), inner.operand_a(), inner.constant() + a2->constant());
                                    break;
                                case MICHAELCC_LINEAR_A_SUBTRACT:
                                    folded = std::make_unique<a2_instruction>(a2->type(), a2->destination(), inner.operand_a(), inner.constant() + a2->constant());
                                    break;
                                case MICHAELCC_LINEAR_A_MULTIPLY:
                                    folded = std::make_unique<a2_instruction>(a2->type(), a2->destination(), inner.operand_a(), inner.constant() * a2->constant());
                                    break;
                                default: break;
                                }
                            } else if (inner.type() == MICHAELCC_LINEAR_A_ADD && a2->type() == MICHAELCC_LINEAR_A_SUBTRACT) {
                                folded = std::make_unique<a2_instruction>(MICHAELCC_LINEAR_A_ADD, a2->destination(), inner.operand_a(), inner.constant() - a2->constant());
                            } else if (inner.type() == MICHAELCC_LINEAR_A_SUBTRACT && a2->type() == MICHAELCC_LINEAR_A_ADD) {
                                folded = std::make_unique<a2_instruction>(MICHAELCC_LINEAR_A_SUBTRACT, a2->destination(), inner.operand_a(), inner.constant() - a2->constant());
                            }

                            if (folded.has_value()) {
                                a2_defs.erase(a2->destination());
                                auto* new_a2 = dynamic_cast<const a2_instruction*>(folded.value().get());
                                a2_defs.insert({ new_a2->destination(), *new_a2 });
                                new_instructions.emplace_back(std::move(folded.value()));
                                changed = true;
                                continue;
                            }
                        }
                    }

                    // fold load_memory(a2(base, c1), offset) → load_memory(base, offset ± c1)
                    if (auto* load = dynamic_cast<const load_memory*>(inst.get())) {
                        auto it = a2_defs.find(load->source_address());
                        if (it != a2_defs.end()) {
                            auto& inner = it->second;
                            if (inner.type() == MICHAELCC_LINEAR_A_ADD) {
                                new_instructions.emplace_back(std::make_unique<load_memory>(load->destination(), inner.operand_a(), load->offset() + static_cast<int64_t>(inner.constant())));
                                changed = true;
                                continue;
                            } else if (inner.type() == MICHAELCC_LINEAR_A_SUBTRACT) {
                                new_instructions.emplace_back(std::make_unique<load_memory>(load->destination(), inner.operand_a(), load->offset() - static_cast<int64_t>(inner.constant())));
                                changed = true;
                                continue;
                            }
                        }
                    }

                    // fold store_memory(a2(base, c1), value, offset) → store_memory(base, value, offset ± c1)
                    if (auto* store = dynamic_cast<const store_memory*>(inst.get())) {
                        auto it = a2_defs.find(store->source_address());
                        if (it != a2_defs.end()) {
                            auto& inner = it->second;
                            if (inner.type() == MICHAELCC_LINEAR_A_ADD) {
                                new_instructions.emplace_back(std::make_unique<store_memory>(inner.operand_a(), store->value(), store->offset() + static_cast<int64_t>(inner.constant())));
                                changed = true;
                                continue;
                            } else if (inner.type() == MICHAELCC_LINEAR_A_SUBTRACT) {
                                new_instructions.emplace_back(std::make_unique<store_memory>(inner.operand_a(), store->value(), store->offset() - static_cast<int64_t>(inner.constant())));
                                changed = true;
                                continue;
                            }
                        }
                    }

                    new_instructions.emplace_back(std::move(inst));
                }
                block.replace_instructions(std::move(new_instructions));
            }
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
