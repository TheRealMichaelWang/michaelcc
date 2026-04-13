#ifndef MICHAELCC_LINEAR_OPTIMIZATION_PHI_HPP
#define MICHAELCC_LINEAR_OPTIMIZATION_PHI_HPP

#include "linear/ir.hpp"
#include "linear/allocators/frame_allocator.hpp"

// run these after running phi::eliminate_redundant_copies
namespace michaelcc::linear::optimization::postphi {
    // colors vregs and assigns them to registers
    void register_allocation(translation_unit& unit, allocators::frame_allocator& frame_allocator);

    // this is a copy-propagation pass to run AFTER phi-node elimination
    void eliminate_redundant_copies(translation_unit& unit);

    // fold a2 address chains and sink constants into load/store offsets
    void simplify_frame_arithmetic(translation_unit& unit);
}

#endif