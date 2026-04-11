#ifndef MICHAELCC_LINEAR_OPTIMIZATION_GVN_HPP
#define MICHAELCC_LINEAR_OPTIMIZATION_GVN_HPP

#include "linear/ir.hpp"
#include "linear/optimization.hpp"
#include <optional>
#include <unordered_map>
#include <utility>

namespace michaelcc {
    namespace linear {
        namespace optimization {
            // This is the global value numbering pass
            class gvn_pass final : public pass {
            private:
                class instruction_hasher : public instruction_dispatcher<std::optional<size_t>> {
                private:
                    static void hash_combine(size_t& seed, size_t value) noexcept;
                    static size_t hash_virtual_register(virtual_register v) noexcept;
                    static size_t hash_register_word(const register_word& w) noexcept;
                    static size_t hash_function_parameter(const function_parameter& p) noexcept;
                    
                    static size_t tag(size_t kind) noexcept {
                        return kind;
                    }

                protected:
                    std::optional<size_t> handle_default(const instruction& node) override {
                        (void)node;
                        return std::nullopt;
                    }

                    std::optional<size_t> dispatch(const a_instruction& node) override;
                    std::optional<size_t> dispatch(const a2_instruction& node) override;
                    std::optional<size_t> dispatch(const u_instruction& node) override;
                    std::optional<size_t> dispatch(const c_instruction& node) override;
                    std::optional<size_t> dispatch(const init_register& node) override;
                    std::optional<size_t> dispatch(const load_memory& node) override;
                    std::optional<size_t> dispatch(const alloca_instruction& node) override;
                    std::optional<size_t> dispatch(const valloca_instruction& node) override;
                    std::optional<size_t> dispatch(const load_parameter& node) override;
                    std::optional<size_t> dispatch(const load_effective_address& node) override;
                };

                class instruction_comparator : public instruction_dispatcher<bool> {
                private:
                    const instruction* m_compare_to;

                public:
                    instruction_comparator(const instruction* compare_to) : m_compare_to(compare_to) {}

                protected:
                    bool handle_default(const instruction& node) override {
                        (void)node;
                        return false;
                    }

                    bool dispatch(const a_instruction& node) override;
                    bool dispatch(const a2_instruction& node) override;
                    bool dispatch(const u_instruction& node) override;
                    bool dispatch(const c_instruction& node) override;
                    bool dispatch(const init_register& node) override;
                    bool dispatch(const load_memory& node) override;
                    bool dispatch(const alloca_instruction& node) override;
                    bool dispatch(const valloca_instruction& node) override;
                    bool dispatch(const load_parameter& node) override;
                    bool dispatch(const load_effective_address& node) override;
                };

                std::unordered_map<size_t, std::pair<instruction*, size_t>> m_instruction_cache;

            public:
                void prescan(const translation_unit& unit) override;
                bool optimize(translation_unit& unit) override;
                void reset() override {}
            };
        }
    }
}

#endif
