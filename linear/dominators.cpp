#include "linear/dominators.hpp"
#include <functional>
#include <unordered_set>
#include <unordered_map>

namespace michaelcc::linear {

void compute_dominators(translation_unit& unit) {
    for (const auto& func : unit.function_definitions) {
        size_t entry = func->entry_block_id();

        std::unordered_set<size_t> visited;
        std::vector<size_t> postorder;

        std::function<void(size_t)> dfs = [&](size_t id) {
            if (visited.count(id)) return;
            visited.insert(id);
            for (size_t succ : unit.blocks.at(id).successor_block_ids())
                dfs(succ);
            postorder.push_back(id);
        };
        dfs(entry);

        std::vector<size_t> rpo(postorder.rbegin(), postorder.rend());

        std::unordered_map<size_t, size_t> rpo_index;
        for (size_t i = 0; i < rpo.size(); i++)
            rpo_index[rpo[i]] = i;

        auto intersect = [&](size_t b1, size_t b2, const std::unordered_map<size_t, size_t>& idom) -> size_t {
            while (b1 != b2) {
                while (rpo_index[b1] > rpo_index[b2])
                    b1 = idom.at(b1);
                while (rpo_index[b2] > rpo_index[b1])
                    b2 = idom.at(b2);
            }
            return b1;
        };

        std::unordered_map<size_t, size_t> idom;
        idom[entry] = entry;
        bool changed = true;

        while (changed) {
            changed = false;
            for (size_t b : rpo) {
                if (b == entry) continue;

                const auto& preds = unit.blocks.at(b).predecessor_block_ids();

                size_t new_idom = SIZE_MAX;
                for (size_t p : preds) {
                    if (idom.count(p)) { new_idom = p; break; }
                }
                if (new_idom == SIZE_MAX) continue;

                for (size_t p : preds) {
                    if (p == new_idom) continue;
                    if (idom.count(p))
                        new_idom = intersect(p, new_idom, idom);
                }

                if (!idom.count(b) || idom[b] != new_idom) {
                    idom[b] = new_idom;
                    changed = true;
                }
            }
        }

        std::unordered_map<size_t, std::vector<size_t>> children;
        for (const auto& [block_id, parent_id] : idom) {
            if (block_id != parent_id)
                children[parent_id].push_back(block_id);
        }

        for (const auto& [block_id, parent_id] : idom) {
            std::optional<size_t> idom_id = (block_id != parent_id) ? std::optional(parent_id) : std::nullopt;
            unit.blocks.at(block_id).set_dominator_info(std::move(idom_id), std::move(children[block_id]));
        }
    }
}

bool is_dominated_by(const translation_unit& unit, size_t dominator_block_id, size_t dominated_block_id) {
    std::optional<size_t> current_block_id = dominated_block_id;
    while (current_block_id.has_value()) {
        if (current_block_id.value() == dominator_block_id) return true;
        
        current_block_id = unit.blocks.at(current_block_id.value()).immediate_dominator_block_id();
    }
    return false;
}

} // namespace michaelcc::linear
