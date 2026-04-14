#include "linear/optimization/phi.hpp"
#include "linear/optimization/copy_prop.hpp"
#include "linear/allocators/register_allocator.hpp"
#include "linear/allocators/register_spiller.hpp"
#include "linear/allocators/frame_allocator.hpp"
#include "linear/allocators/remove_phi.hpp"
#include <algorithm>
#include <optional>

namespace {

using namespace michaelcc::linear;
using copy_map_t = std::unordered_map<virtual_register, virtual_register>;

bool same_color(virtual_register a, virtual_register b,
                const translation_unit& unit) {
    return unit.vreg_colors.contains(a)
        && unit.vreg_colors.contains(b)
        && unit.vreg_colors.at(a) == unit.vreg_colors.at(b);
}

virtual_register resolve(virtual_register vreg, const copy_map_t& map) {
    auto current = vreg;
    for (int depth = 0; depth < 32; ++depth) {
        auto it = map.find(current);
        if (it == map.end() || it->second == current) break;
        current = it->second;
    }
    return current;
}

void invalidate_color(copy_map_t& map, register_t color,
                      const translation_unit& unit) {
    for (auto it = map.begin(); it != map.end(); ) {
        bool src_hit = unit.vreg_colors.contains(it->second)
                    && unit.vreg_colors.at(it->second) == color;
        bool dst_hit = unit.vreg_colors.contains(it->first)
                    && unit.vreg_colors.at(it->first) == color;
        if (src_hit || dst_hit)
            it = map.erase(it);
        else
            ++it;
    }
}

void invalidate_for_def(copy_map_t& map, virtual_register dest,
                        const translation_unit& unit) {
    map.erase(dest);
    if (unit.vreg_colors.contains(dest))
        invalidate_color(map, unit.vreg_colors.at(dest), unit);
}

void invalidate_for_call(copy_map_t& map, const function_call& call,
                         const translation_unit& unit) {
    for (const auto& csr : call.caller_saved_registers()) {
        if (unit.vreg_colors.contains(csr))
            invalidate_color(map, unit.vreg_colors.at(csr), unit);
    }
}

copy_map_t simulate_block(const basic_block& block,
                          const copy_map_t& entry,
                          const translation_unit& unit) {
    auto map = entry;
    for (const auto& inst : block.instructions()) {
        if (auto* copy = dynamic_cast<const c_instruction*>(inst.get())) {
            if (copy->type() == MICHAELCC_LINEAR_C_COPY_INIT) {
                auto src = resolve(copy->source(), map);
                invalidate_for_def(map, copy->destination(), unit);
                map[copy->destination()] = src;
                continue;
            }
        }
        if (auto dest = inst->destination_register())
            invalidate_for_def(map, *dest, unit);
        if (auto* call = dynamic_cast<const function_call*>(inst.get()))
            invalidate_for_call(map, *call, unit);
    }
    return map;
}

copy_map_t intersect(const copy_map_t& a, const copy_map_t& b) {
    copy_map_t result;
    for (const auto& [k, v] : a) {
        auto it = b.find(k);
        if (it != b.end() && it->second == v)
            result[k] = v;
    }
    return result;
}

std::optional<a2_instruction> try_fold_a2(const a2_instruction& outer,
                                          const a2_instruction& inner) {
    auto outer_op = outer.type();
    auto inner_op = inner.type();
    uint64_t c;

    if (outer_op == inner_op) {
        switch (outer_op) {
        case MICHAELCC_LINEAR_A_ADD:
        case MICHAELCC_LINEAR_A_SUBTRACT:
            c = inner.constant() + outer.constant();
            break;
        case MICHAELCC_LINEAR_A_SIGNED_MULTIPLY:
        case MICHAELCC_LINEAR_A_UNSIGNED_MULTIPLY:
            c = inner.constant() * outer.constant();
            break;
        default:
            return std::nullopt;
        }
        return a2_instruction(inner_op, outer.destination(),
                              inner.operand_a(), c);
    }

    if (inner_op == MICHAELCC_LINEAR_A_ADD
        && outer_op == MICHAELCC_LINEAR_A_SUBTRACT) {
        return a2_instruction(MICHAELCC_LINEAR_A_ADD, outer.destination(),
                              inner.operand_a(),
                              inner.constant() - outer.constant());
    }
    if (inner_op == MICHAELCC_LINEAR_A_SUBTRACT
        && outer_op == MICHAELCC_LINEAR_A_ADD) {
        return a2_instruction(MICHAELCC_LINEAR_A_SUBTRACT, outer.destination(),
                              inner.operand_a(),
                              inner.constant() - outer.constant());
    }

    return std::nullopt;
}

std::optional<int64_t> get_additive_offset(const a2_instruction& a2) {
    if (a2.type() == MICHAELCC_LINEAR_A_ADD)
        return static_cast<int64_t>(a2.constant());
    if (a2.type() == MICHAELCC_LINEAR_A_SUBTRACT)
        return -static_cast<int64_t>(a2.constant());
    return std::nullopt;
}

}

namespace michaelcc::linear::optimization::postphi {

void copy_prop_pass::prescan(const translation_unit& unit) {
    std::unordered_map<size_t, copy_map_t> exit_maps;

    bool converged = false;
    while (!converged) {
        converged = true;
        for (const auto& [block_id, block] : unit.blocks) {
            copy_map_t entry;
            const auto& preds = block.predecessor_block_ids();

            if (!preds.empty()) {
                bool first = true;
                for (size_t pred_id : preds) {
                    auto it = exit_maps.find(pred_id);
                    if (it == exit_maps.end()) {
                        entry.clear();
                        first = false;
                        break;
                    }
                    if (first) {
                        entry = it->second;
                        first = false;
                    } else {
                        entry = intersect(entry, it->second);
                    }
                }
                if (first) entry.clear();
            }

            auto exit = simulate_block(block, entry, unit);

            if (!exit_maps.contains(block_id) || exit_maps.at(block_id) != exit) {
                exit_maps[block_id] = std::move(exit);
                m_entry_maps[block_id] = std::move(entry);
                converged = false;
            } else {
                m_entry_maps[block_id] = std::move(entry);
            }
        }
    }
}

bool copy_prop_pass::optimize(translation_unit& unit) {
    bool changed = false;

    for (auto& [block_id, block] : unit.blocks) {
        copy_map_t map = m_entry_maps.contains(block_id)
                       ? m_entry_maps.at(block_id) : copy_map_t{};

        auto released = block.release_instructions();
        std::vector<std::unique_ptr<instruction>> out;
        out.reserve(released.size());

        for (auto& inst : released) {
            replace_operands_transform transform(map);
            auto rewritten = transform(*inst);
            if (rewritten) {
                changed = true;
                inst = std::move(rewritten);
            }

            if (auto* copy = dynamic_cast<const c_instruction*>(inst.get())) {
                if (copy->type() == MICHAELCC_LINEAR_C_COPY_INIT) {
                    auto src = resolve(copy->source(), map);

                    if (same_color(src, copy->destination(), unit)) {
                        invalidate_for_def(map, copy->destination(), unit);
                        map[copy->destination()] = src;
                        changed = true;
                        continue;
                    }

                    if (src != copy->source()) {
                        inst = std::make_unique<c_instruction>(
                            MICHAELCC_LINEAR_C_COPY_INIT,
                            copy->destination(), src);
                        changed = true;
                    }

                    invalidate_for_def(map, copy->destination(), unit);
                    map[copy->destination()] = src;
                    out.emplace_back(std::move(inst));
                    continue;
                }
            }

            if (auto dest = inst->destination_register())
                invalidate_for_def(map, *dest, unit);
            if (auto* call = dynamic_cast<const function_call*>(inst.get()))
                invalidate_for_call(map, *call, unit);

            out.emplace_back(std::move(inst));
        }
        block.replace_instructions(std::move(out));
    }

    std::unordered_map<virtual_register, size_t> use_counts;
    for (const auto& [block_id, block] : unit.blocks)
        for (const auto& inst : block.instructions())
            for (const auto& op : inst->operand_registers())
                use_counts[op]++;

    for (auto& [block_id, block] : unit.blocks) {
        auto released = block.release_instructions();
        std::vector<std::unique_ptr<instruction>> kept;
        kept.reserve(released.size());
        for (auto& inst : released) {
            if (auto* copy = dynamic_cast<const c_instruction*>(inst.get())) {
                if (copy->type() == MICHAELCC_LINEAR_C_COPY_INIT
                    && use_counts[copy->destination()] == 0) {
                    changed = true;
                    continue;
                }
            }
            kept.emplace_back(std::move(inst));
        }
        block.replace_instructions(std::move(kept));
    }

    return changed;
}


void frame_arithmetic_pass::prescan(const translation_unit& unit) {
    for (const auto& [block_id, block] : unit.blocks)
        for (const auto& inst : block.instructions())
            if (auto* a2 = dynamic_cast<const a2_instruction*>(inst.get()))
                m_a2_defs.insert({ a2->destination(), *a2 });
}

bool frame_arithmetic_pass::optimize(translation_unit& unit) {
    bool changed = false;

    for (auto& [block_id, block] : unit.blocks) {
        auto released = block.release_instructions();
        std::vector<std::unique_ptr<instruction>> out;
        out.reserve(released.size());

        for (auto& inst : released) {
            if (auto* a2 = dynamic_cast<const a2_instruction*>(inst.get())) {
                auto inner_it = m_a2_defs.find(a2->operand_a());
                if (inner_it != m_a2_defs.end()) {
                    if (auto folded = try_fold_a2(*a2, inner_it->second)) {
                        m_a2_defs.erase(a2->destination());
                        m_a2_defs.insert({ folded->destination(), *folded });
                        out.emplace_back(
                            std::make_unique<a2_instruction>(*folded));
                        changed = true;
                        continue;
                    }
                }
            }

            if (auto* load = dynamic_cast<const load_memory*>(inst.get())) {
                auto it = m_a2_defs.find(load->source_address());
                if (it != m_a2_defs.end()) {
                    if (auto delta = get_additive_offset(it->second)) {
                        out.emplace_back(std::make_unique<load_memory>(
                            load->destination(),
                            it->second.operand_a(),
                            load->offset() + *delta));
                        changed = true;
                        continue;
                    }
                }
            }

            if (auto* store = dynamic_cast<const store_memory*>(inst.get())) {
                auto it = m_a2_defs.find(store->source_address());
                if (it != m_a2_defs.end()) {
                    if (auto delta = get_additive_offset(it->second)) {
                        out.emplace_back(std::make_unique<store_memory>(
                            it->second.operand_a(),
                            store->value(),
                            store->offset() + *delta));
                        changed = true;
                        continue;
                    }
                }
            }

            out.emplace_back(std::move(inst));
        }
        block.replace_instructions(std::move(out));
    }

    return changed;
}


void register_allocation(translation_unit& unit,
                         allocators::frame_allocator& frame_allocator) {
    std::vector<std::unique_ptr<pass>> postphi_passes;
    postphi_passes.emplace_back(std::make_unique<copy_prop_pass>());
    postphi_passes.emplace_back(std::make_unique<frame_arithmetic_pass>());

    for (;;) {
        allocators::register_allocator register_allocator(unit);

        auto spilled_vregs = register_allocator.allocate();
        if (spilled_vregs.empty()) {
            transform(unit, postphi_passes);
            break;
        }

        allocators::register_spiller register_spiller(unit, spilled_vregs);
        register_spiller.spill();
        frame_allocator.allocate();
        transform(unit, postphi_passes);
    }
}

}

namespace michaelcc::linear::allocators {

struct phi_copy {
    virtual_register dest;
    virtual_register source;
};

static std::vector<phi_copy> sequentialize_parallel_copies(
    std::vector<phi_copy> copies,
    linear::translation_unit& unit
) {
    std::vector<phi_copy> result;

    while (!copies.empty()) {
        auto safe = std::find_if(copies.begin(), copies.end(),
            [&](const phi_copy& c) {
                return std::none_of(copies.begin(), copies.end(),
                    [&](const phi_copy& other) {
                        return &other != &c && other.source == c.dest;
                    });
            });

        if (safe != copies.end()) {
            result.push_back(*safe);
            copies.erase(safe);
        } else {
            auto& first = copies.front();
            auto temp = unit.new_vreg(first.source.reg_size,
                                      first.source.reg_class);
            result.push_back({ temp, first.source });
            first.source = temp;
        }
    }

    return result;
}

void remove_phi_nodes(linear::translation_unit& unit) {
    std::unordered_map<size_t, std::vector<phi_copy>> pending_copies;

    for (auto& [block_id, block] : unit.blocks) {
        auto released_instructions = block.release_instructions();
        std::vector<std::unique_ptr<instruction>> new_instructions;
        new_instructions.reserve(released_instructions.size());

        for (auto& instruction : released_instructions) {
            if (auto phi = dynamic_cast<const phi_instruction*>(instruction.get())) {
                for (const auto& value : phi->values()) {
                    if (phi->destination() == value.vreg) continue;
                    assert(unit.blocks.contains(value.block_id));
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

    for (auto& [pred_block_id, copies] : pending_copies) {
        auto sequentialized = sequentialize_parallel_copies(
            std::move(copies), unit);
        auto& pred_block = unit.blocks.at(pred_block_id);
        for (auto& copy : sequentialized) {
            pred_block.phi_add_instruction(
                std::make_unique<linear::c_instruction>(
                    linear::MICHAELCC_LINEAR_C_COPY_INIT,
                    copy.dest, copy.source));
        }
    }
}

}
