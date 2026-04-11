#ifndef MICHAELCC_LINEAR_DOMINATORS_HPP
#define MICHAELCC_LINEAR_DOMINATORS_HPP

#include "ir.hpp"

namespace michaelcc::linear {
    void compute_dominators(translation_unit& unit);

    bool is_dominated_by(const translation_unit& unit, size_t dominator_block_id, size_t dominated_block_id);
}

#endif
