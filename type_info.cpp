#include "logic/type_info.hpp"
#include <algorithm>
#include <numeric>

namespace michaelcc {
    size_t type_layout_calculator::get_int_type_size(const typing::int_type& type, const platform_info& info) {
        switch (type.type_class()) {
            case typing::CHAR_INT_CLASS:
                return 1;
            case typing::SHORT_INT_CLASS:
                return info.short_size;
            case typing::INT_INT_CLASS:
                return (type.int_qualifiers() & typing::LONG_INT_QUALIFIER)
                    ? info.long_size
                    : info.int_size;
            case typing::LONG_INT_CLASS:
                return info.long_long_size;
            default:
                throw std::runtime_error("Invalid int type class");
        }
    }

    bool type_layout_calculator::must_alloca(const typing::qual_type type) noexcept {
        std::shared_ptr<typing::union_type> union_type = std::dynamic_pointer_cast<typing::union_type>(type.type());
        if (union_type) {
            for (auto& member : union_type->members()) {
                if (must_alloca(member.member_type)) {
                    return true;
                }
            }
            return false;
        }
        return type.is_same_type<typing::struct_type>() || (*this)(*type.type()).size * 8 > m_platform_info.max_register_size();
    }

    bool typing::int_type::is_assignable_from(const typing::base_type& other, const platform_info& platform) const {
        if (typeid(other) != typeid(*this)) {
            if (dynamic_cast<const typing::enum_type*>(&other)) {
                return true;
            }
            return false;
        }

        const int_type& other_int = static_cast<const int_type&>(other);
        
        // Strict signedness matching - unsigned -> signed requires explicit cast
        if (is_unsigned() != other_int.is_unsigned()) {
            return false;
        }

        // Use type_layout_calculator to get actual sizes for comparison
        size_t this_size = type_layout_calculator::get_int_type_size(*this, platform);
        size_t other_size = type_layout_calculator::get_int_type_size(other_int, platform);
        
        // Allow smaller or equal size to be assigned to this type
        return this_size >= other_size;
    }

    const type_layout_info type_layout_calculator::dispatch(typing::int_type& type) {
        size_t size;
        switch (type.type_class()) {
            case typing::CHAR_INT_CLASS:
                size = 1;
                break;
            case typing::SHORT_INT_CLASS:
                size = m_platform_info.short_size;
                break;
            case typing::INT_INT_CLASS:
                size = (type.int_qualifiers() & typing::LONG_INT_QUALIFIER)
                    ? m_platform_info.long_size
                    : m_platform_info.int_size;
                break;
            case typing::LONG_INT_CLASS:
                size = m_platform_info.long_long_size;
                break;
            default:
                throw std::runtime_error("Invalid int type class");
        }
        return { size, std::min<size_t>(size, m_platform_info.max_alignment) };
    }

    const type_layout_info type_layout_calculator::dispatch(typing::float_type& type) {
        switch (type.type_class()) {
            case typing::FLOAT_FLOAT_CLASS:
                return { 4, std::min<size_t>(4, m_platform_info.max_alignment) };
            case typing::DOUBLE_FLOAT_CLASS:
                return { 8, std::min<size_t>(8, m_platform_info.max_alignment) };
        }
        throw std::runtime_error("Invalid float type class");
    }

    const type_layout_info type_layout_calculator::dispatch(typing::struct_type& type) {
        if (m_declared_info.contains(&type)) {
            return m_declared_info.at(&type);
        }

        std::vector<size_t> original_indices(type.fields().size());
        std::iota(original_indices.begin(), original_indices.end(), 0);
        if (m_platform_info.optimize_struct_layout) {
            // Sort by alignment (descending) to minimize padding
            std::sort(original_indices.begin(), original_indices.end(), [this, &type](size_t a, size_t b) {
                const type_layout_info a_layout = (*this)(*type.fields().at(a).member_type.type());
                const type_layout_info b_layout = (*this)(*type.fields().at(b).member_type.type());
                return a_layout.alignment > b_layout.alignment;
            });
        }
        
        size_t size = 0;
        size_t alignment = 1;
        size_t offset = 0;
        size_t max_alignment = 1;

        std::vector<size_t> field_offsets(type.fields().size());
        for (const auto& index : original_indices) {
            const auto& field = type.fields().at(index);
            const type_layout_info field_layout = (*this)(*field.member_type.type());

            // Pad to field alignment
            size_t padding = (field_layout.alignment - (offset % field_layout.alignment)) % field_layout.alignment;
            offset += padding;
            field_offsets[index] = offset;
            offset += field_layout.size;
            
            max_alignment = std::max(max_alignment, field_layout.alignment);
        }

        // Pad final size to struct alignment
        size_t final_padding = (max_alignment - (offset % max_alignment)) % max_alignment;
        size = offset + final_padding;
        alignment = max_alignment;

        type.implement_field_offsets(field_offsets);
        m_declared_info.emplace(&type, type_layout_info{.size=size, .alignment=alignment });
        return m_declared_info.at(&type);
    }

    const type_layout_info type_layout_calculator::dispatch(typing::union_type& type) {
        if (m_declared_info.contains(&type)) {
            return m_declared_info.at(&type);
        }

        size_t max_size = 0;
        size_t max_alignment = 1;

        for (const auto& member : type.members()) {
            const type_layout_info member_layout = (*this)(*member.member_type.type());
            max_size = std::max(max_size, member_layout.size);
            max_alignment = std::max(max_alignment, member_layout.alignment);
        }

        max_alignment = std::min<size_t>(max_alignment, m_platform_info.max_alignment);
        
        // Pad size to alignment
        size_t size = max_size + (max_alignment - (max_size % max_alignment)) % max_alignment;
        
        m_declared_info.emplace(&type, type_layout_info{.size=size, .alignment=max_alignment });
        return m_declared_info.at(&type);
    }
}
