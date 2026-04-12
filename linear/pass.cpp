#include "linear/pass.hpp"
#include "linear/dominators.hpp"
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

                any_pass_mutated |= pass->optimize(unit);

                pass->reset();
            }

            if (any_pass_mutated) {
                compute_dominators(unit);
            }
            passes_run++;
        } while (any_pass_mutated);

        return true;
    }
}