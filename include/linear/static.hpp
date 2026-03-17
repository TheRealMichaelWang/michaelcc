#ifndef MICHAELCC_LINEAR_STATIC_HPP
#define MICHAELCC_LINEAR_STATIC_HPP

#include "linear/registers.hpp"
#include "logic/ir.hpp"
#include "logic/type_info.hpp"
#include "platform.hpp"
#include <tuple>

namespace michaelcc {
    namespace linear {
        namespace static_storage {
            struct bss_allocation {
                const std::string label;
                const type_layout_info layout;
            };

            struct data_word {
                std::optional<std::string> label_ref;
                const register_word value;
                const word_size size;
            };

            struct data_allocation {
                std::string label;
                const std::vector<data_word> data_words;
                const type_layout_info layout;
            };

            struct static_sections {
                std::vector<bss_allocation> bss_allocations;
                std::vector<data_allocation> data_allocations;
            };

            class is_default_initialized : public logic::const_expression_dispatcher<std::optional<type_layout_info>> {
            private:
                const platform_info m_platform_info;

            public:
                is_default_initialized(const platform_info& platform_info) : m_platform_info(platform_info) {}

            protected:
                std::optional<type_layout_info> dispatch(const logic::integer_constant& node) override {
                    if (node.value() == 0) {
                        type_layout_calculator calculator(m_platform_info);
                        return calculator(*node.get_type().type());
                    }
                    return std::nullopt;
                }
                
                std::optional<type_layout_info> dispatch(const logic::floating_constant& node) override {
                    if (node.value() == 0.0) {
                        type_layout_calculator calculator(m_platform_info);
                        return calculator(*node.get_type().type());
                    }
                    return std::nullopt;
                }

                std::optional<type_layout_info> dispatch(const logic::type_cast& node) override {
                    return (*this)(*node.operand());
                }

                std::optional<type_layout_info> dispatch(const logic::array_initializer& node) override {
                    for (const auto& initializer : node.initializers()) {
                        if (!(*this)(*initializer)) {
                            return std::nullopt;
                        }
                    }
                    type_layout_calculator calculator(m_platform_info);
                    auto layout = calculator(*node.get_type().type());
                    return type_layout_info{
                        .size = layout.size * node.initializers().size(),
                        .alignment = layout.alignment
                    };
                }

                std::optional<type_layout_info> dispatch(const logic::struct_initializer& node) override {
                    for (const auto& initializer : node.initializers()) {
                        if (!(*this)(*initializer.initializer)) {
                            return std::nullopt;
                        }
                    }
                    type_layout_calculator calculator(m_platform_info);
                    return calculator(*node.get_type().type());
                }

                std::optional<type_layout_info> dispatch(const logic::union_initializer& node) override {
                    if (!(*this)(*node.initializer())) {
                        return std::nullopt;
                    }
                    type_layout_calculator calculator(m_platform_info);
                    return calculator(*node.get_type().type());
                }

                std::optional<type_layout_info> handle_default(const logic::expression&) override {
                    return std::nullopt;
                }
            };

            inline bool can_allocate_bss(const logic::variable_declaration& declaration, const platform_info& platform_info) {
                is_default_initialized is_default_initialized(platform_info);
                return is_default_initialized(*declaration.initializer()).has_value();
            }

            class data_section_builder : public logic::const_expression_dispatcher<void> {
            private:
                std::string m_label;
                std::vector<data_word> m_data_words;
                size_t current_alignment;
                size_t current_size;
                std::vector<data_allocation> m_sub_allocations;

                const platform_info m_platform_info;

            public:
                data_section_builder(const platform_info& platform_info, const std::string& label) 
                : m_label(label), m_data_words(), current_alignment(1), current_size(0), m_platform_info(platform_info) {}

                std::vector<data_allocation> build(const logic::expression& node);

            private:
                void dispatch_initializer(const logic::expression& node, size_t id);

            protected:
                void dispatch(const logic::integer_constant& node) override;
                void dispatch(const logic::floating_constant& node) override;
                void dispatch(const logic::string_constant& node) override;

                void dispatch(const logic::enumerator_literal& node) override;

                void dispatch(const logic::array_initializer& node) override;
                void dispatch(const logic::struct_initializer& node) override;
                void dispatch(const logic::union_initializer& node) override;
            };

            register_word const_to_regword(int64_t value, word_size size, bool is_signed);
            std::tuple<register_word, word_size> int_literal_to_regword(const logic::integer_constant& node, const platform_info& platform_info);
        }
    }
}

#endif