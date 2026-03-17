#include "linear/static.hpp"
#include "logic/type_info.hpp"
#include <cassert>

namespace michaelcc {
    namespace linear {
        namespace static_storage {
            register_word const_to_regword(int64_t value, word_size size, bool is_signed) {
                register_word result;
                switch (size) {
                    case word_size::MICHAELCC_WORD_SIZE_BYTE:
                        if (is_signed) { result.sbyte = static_cast<int8_t>(value); } else { result.ubyte = static_cast<uint8_t>(value); }
                        break;
                    case word_size::MICHAELCC_WORD_SIZE_UINT16:
                        if (is_signed) { result.int16 = static_cast<int16_t>(value); } else { result.uint16 = static_cast<uint16_t>(value); }
                        break;
                    case word_size::MICHAELCC_WORD_SIZE_UINT32:
                        if (is_signed) { result.int32 = static_cast<int32_t>(value); } else { result.uint32 = static_cast<uint32_t>(value); }
                        break;
                    case word_size::MICHAELCC_WORD_SIZE_UINT64:
                        if (is_signed) { result.int64 = static_cast<int64_t>(value); } else { result.uint64 = static_cast<uint64_t>(value); }
                        break;
                    default:
                        throw std::runtime_error("Invalid word size");
                }
                return result;
            }

            std::tuple<register_word, word_size> int_literal_to_regword(const logic::integer_constant& node, const platform_info& platform_info) {
                std::shared_ptr<typing::int_type> int_type = std::static_pointer_cast<typing::int_type>(node.get_type().type());
                auto reg_size = type_layout_calculator::get_int_type_size(
                    *int_type, 
                    platform_info
                );
                return std::make_tuple(const_to_regword(node.value(), reg_size, int_type->int_qualifiers() & typing::SIGNED_INT_QUALIFIER), reg_size);
            }

            void data_section_builder::dispatch(const logic::integer_constant& node) {
                auto [value, reg_size] = int_literal_to_regword(node, m_platform_info);
                m_data_words.push_back(data_word{ .value = value, .size = reg_size });
                current_size += static_cast<size_t>(reg_size) / 8;

                current_alignment = std::max(current_alignment, static_cast<size_t>(reg_size) / 8);
            }

            void data_section_builder::dispatch(const logic::floating_constant& node) {
                std::shared_ptr<typing::float_type> float_type = std::static_pointer_cast<typing::float_type>(node.get_type().type());

                linear::register_word value;
                if (float_type->type_class() == typing::DOUBLE_FLOAT_CLASS) {
                    value.float64 = node.value();
                }
                else {
                    assert(float_type->type_class() == typing::FLOAT_FLOAT_CLASS);
                    value.float32 = node.value();
                }

                auto reg_size = float_type->type_class() == typing::DOUBLE_FLOAT_CLASS 
                    ? linear::word_size::MICHAELCC_WORD_SIZE_UINT64 
                    : linear::word_size::MICHAELCC_WORD_SIZE_UINT32;

                m_data_words.push_back(data_word{ .value = value, .size = reg_size });
                current_size += static_cast<size_t>(reg_size) / 8;

                current_alignment = std::max(current_alignment, static_cast<size_t>(reg_size) / 8);
            }

            void data_section_builder::dispatch(const logic::string_constant& node) {
                std::string label = "@string_" + std::to_string(node.index());
                m_data_words.push_back(data_word{ .label_ref = label, .size = m_platform_info.pointer_size });
                current_size += static_cast<size_t>(m_platform_info.pointer_size) / 8;
            
                current_alignment = std::max(current_alignment, static_cast<size_t>(m_platform_info.pointer_size) / 8);
            }

            void data_section_builder::dispatch(const logic::enumerator_literal& node) {
                int64_t enumerator_value = node.enumerator().value;

                linear::register_word value = linear::static_storage::const_to_regword(enumerator_value, m_platform_info.int_size, true);

                m_data_words.push_back(data_word{ .value = value, .size = m_platform_info.int_size });
                current_size += static_cast<size_t>(m_platform_info.int_size) / 8;

                current_alignment = std::max(current_alignment, static_cast<size_t>(m_platform_info.int_size) / 8);
            }

            void data_section_builder::dispatch(const logic::array_initializer& node) {
                for (size_t i = 0; i < node.initializers().size(); i++) {
                    dispatch_initializer(*node.initializers()[i], i);
                }
            }

            void data_section_builder::dispatch(const logic::struct_initializer& node) {
                auto struct_type = node.struct_type();
                type_layout_calculator calculator(m_platform_info);

                size_t struct_size = 0;
                for (size_t i = 0; i < node.initializers().size(); i++) {
                    auto field = std::find_if(struct_type->fields().begin(), struct_type->fields().end(), [&](const typing::member& member) {
                        return member.name == node.initializers()[i].member_name;
                    });
                    auto layout = calculator(*field->member_type.type());

                    if (struct_size < field->offset) {
                        for (size_t j = struct_size; j < field->offset; j++) {
                            m_data_words.push_back(data_word{ .value = register_word{ .uint64 = 0 }, .size = linear::word_size::MICHAELCC_WORD_SIZE_BYTE });
                        }
                        struct_size = field->offset;
                    }
                    dispatch_initializer(*node.initializers()[i].initializer, i);

                    struct_size += layout.size;
                }
            }

            void data_section_builder::dispatch(const logic::union_initializer& node) {
                auto union_type = node.union_type();

                type_layout_calculator calculator(m_platform_info);
                auto member_layout = calculator(*node.target_member().member_type.type());
                auto union_layout = calculator(*union_type);
                (*this)(*node.initializer());

                for (size_t i = member_layout.size; i < union_layout.size; i++) {
                    m_data_words.push_back(data_word{ .value = register_word{ .uint64 = 0 }, .size = linear::word_size::MICHAELCC_WORD_SIZE_BYTE });
                }
            }

            void data_section_builder::dispatch_initializer(const logic::expression& node, size_t id) {
                if (auto array_initializer = dynamic_cast<const logic::array_initializer*>(&node)) {
                    std::string label = std::format("array{}_{}", id, m_label);
                    
                    data_section_builder sub_builder(m_platform_info, label);
                    auto nested_allocations = sub_builder.build(node);
                    for (auto& allocation : nested_allocations) {
                        m_sub_allocations.push_back(std::move(allocation));
                    }

                    m_data_words.push_back(data_word{ .label_ref = label, .size = m_platform_info.pointer_size });
                    current_size += static_cast<size_t>(m_platform_info.pointer_size) / 8;
                
                    current_alignment = std::max(current_alignment, static_cast<size_t>(m_platform_info.pointer_size) / 8);
                }
                else {
                    (*this)(node);
                }
            }

            std::vector<data_allocation> data_section_builder::build(const logic::expression& node) {
                (*this)(node);
                current_alignment = std::max(current_alignment, m_platform_info.max_alignment);
                size_t padding = (current_alignment - (current_size % current_alignment)) % current_alignment;
                size_t final_size = current_size + padding;

                for (size_t i = 0; i < padding; i++) {
                    m_data_words.push_back(data_word{ .value = register_word{ .uint64 = 0 }, .size = linear::word_size::MICHAELCC_WORD_SIZE_BYTE });
                }

                auto current = data_allocation{ 
                    .label = m_label, 
                    .data_words = m_data_words, 
                    .layout = type_layout_info{
                        .size = final_size,
                        .alignment = current_alignment
                    }
                };

                m_sub_allocations.push_back(current);
                return m_sub_allocations;
            }
        }
    }
}