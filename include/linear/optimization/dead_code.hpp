#include "linear/optimization.hpp"
#include <unordered_set>

namespace michaelcc::linear::optimization {
    class dead_code_pass final : public pass {
    private:
        std::unordered_set<instruction*> used_instructions;

    public:
        void prescan(const translation_unit& unit) override;
        bool optimize(translation_unit& unit) override;
        void reset() override { used_instructions.clear(); }
    };
}