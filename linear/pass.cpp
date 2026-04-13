#include "linear/pass.hpp"
#include "linear/dominators.hpp"
#include "linear/allocators/remove_phi.hpp"
#include <algorithm>

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
}