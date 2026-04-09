#include "linear/optimization.hpp"
#include <unordered_set>

namespace michaelcc::linear::optimization {
    class dead_instruction_pass final : public pass {
    private:
        std::unordered_set<instruction*> used_instructions;

    public:
        void prescan(const translation_unit& unit) override;
        bool optimize(translation_unit& unit) override;
        void reset() override { used_instructions.clear(); }
    };

    class dead_block_pass final : public pass {
    private:
        std::unordered_set<size_t> used_block_ids;

    public:
        void prescan(const translation_unit& unit) override;
        bool optimize(translation_unit& unit) override;
        void reset() override { used_block_ids.clear(); }
    };
}