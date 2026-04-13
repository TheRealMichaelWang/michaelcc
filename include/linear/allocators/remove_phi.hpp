#ifndef MICHAELCC_LINEAR_ALLOCATORS_REMOVE_PHI_HPP
#define MICHAELCC_LINEAR_ALLOCATORS_REMOVE_PHI_HPP

#include "linear/ir.hpp"

namespace michaelcc::linear::allocators {
    void remove_phi_nodes(linear::translation_unit& translation_unit);
}

#endif