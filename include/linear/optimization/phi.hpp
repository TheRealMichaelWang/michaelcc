#ifndef MICHAELCC_LINEAR_OPTIMIZATION_PHI_HPP
#define MICHAELCC_LINEAR_OPTIMIZATION_PHI_HPP

#include "linear/pass.hpp"
#include "linear/allocators/frame_allocator.hpp"
#include <unordered_map>

namespace michaelcc::linear::optimization::postphi {

    void register_allocation(translation_unit& unit, allocators::frame_allocator& frame_allocator);

    // Cross-block copy propagation for post-phi IR.
    class copy_prop_pass final : public pass {
    private:
        using copy_map_t = std::unordered_map<virtual_register, virtual_register>;
        std::unordered_map<size_t, copy_map_t> m_entry_maps;

    public:
        void prescan(const translation_unit& unit) override;
        bool optimize(translation_unit& unit) override;
        void reset() override { m_entry_maps.clear(); }
    };

    // Frame arithmetic simplification.
    class frame_arithmetic_pass final : public pass {
    private:
        std::unordered_map<virtual_register, a2_instruction> m_a2_defs;

    public:
        void prescan(const translation_unit& unit) override;
        bool optimize(translation_unit& unit) override;
        void reset() override { m_a2_defs.clear(); }
    };
}

#endif
