#include "linear/optimization.hpp"

namespace michaelcc {
    namespace linear {
        namespace optimization {
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

                        result result = pass->optimize(unit);

                        if (result.blocks_to_remove.size() > 0 || result.block_new_instructions.size() > 0) {
                            any_pass_mutated = true;

                            for (size_t block_index : result.blocks_to_remove) {
                                unit.blocks.erase(block_index);
                            }

                            for (auto& [block_index, new_instructions] : result.block_new_instructions) {
                                unit.blocks.at(block_index).replace_instructions(std::move(new_instructions));
                            }
                        }
                    }

                    passes_run++;
                } while (any_pass_mutated);

                return true;
            }
        }
    }
}