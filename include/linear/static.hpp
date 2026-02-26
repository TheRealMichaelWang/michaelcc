#ifndef MICHAELCC_LINEAR_STATIC_HPP
#define MICHAELCC_LINEAR_STATIC_HPP

#include "logic/ir.hpp"
#include <variant>

namespace michaelcc {
    namespace linear {
        namespace static_storage {
            struct bss_allocation {
                const std::string label;
                const size_t size;
                const size_t alignment;
            };

            struct data_word {
                using word = std::variant<
                    uint64_t, uint32_t, uint16_t, uint8_t,
                    float, double,
                    std::string
                >;
                
                const word value;
                const size_t offset;
            };

            struct data_allocation {
                const std::vector<data_word> data_words;
                const size_t alignment;
                const size_t size;
            };

            struct static_sections {
                const std::vector<bss_allocation> bss_allocations;
                const std::vector<data_allocation> data_allocations;
            };

            class is_default_initialized : public logic::const_expression_dispatcher<bool> {
            protected:
                bool dispatch(const logic::integer_constant& node) override {
                    return node.value() == 0;
                }
                
                bool dispatch(const logic::floating_constant& node) override {
                    return node.value() == 0.0;
                }

                bool dispatch(const logic::type_cast& node) override {
                    return (*this)(*node.operand());
                }

                bool dispatch(const logic::variable_reference& node) override {
                    return node.is_global();
                }

                bool dispatch(const logic::array_initializer& node) override {
                    for (const auto& initializer : node.initializers()) {
                        if (!(*this)(*initializer)) {
                            return false;
                        }
                    }
                    return true;
                }

                bool dispatch(const logic::struct_initializer& node) override {
                    for (const auto& initializer : node.initializers()) {
                        if (!(*this)(*initializer.initializer)) {
                            return false;
                        }
                    }
                    return true;
                }

                bool dispatch(const logic::union_initializer& node) override {
                    return (*this)(*node.initializer());
                }

                bool handle_default(const logic::expression&) override {
                    return false;
                }
            };

            inline bool can_allocate_bss(const logic::variable_declaration& declaration) {
                is_default_initialized is_default_initialized;
                return is_default_initialized(*declaration.initializer());
            }
        }
    }
}

#endif