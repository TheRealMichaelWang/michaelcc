#ifndef MICHAELCC_LINEAR_OPTIMIZATION_HPP
#define MICHAELCC_LINEAR_OPTIMIZATION_HPP

#include <vector>
#include <unordered_map>
#include <memory>
#include "linear/ir.hpp"

namespace michaelcc {
    namespace linear {
        namespace optimization {
            struct result {
                std::vector<size_t> blocks_to_remove;
                std::unordered_map<size_t, std::vector<std::unique_ptr<instruction>>> block_new_instructions;
            };

            class pass {
            public:
                // prescan the IR to gather information about the blocks to optimize
                virtual void prescan(const translation_unit& unit) = 0;

                // optimize the IR
                virtual result optimize(translation_unit& unit) = 0;

                // reset the state of the pass
                virtual void reset() = 0;
            };

            bool transform(translation_unit& unit, std::vector<std::unique_ptr<pass>>& passes, int max_passes=1000);
        }
    }
}

#endif