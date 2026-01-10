#include "compiler.hpp"
#include "typing.hpp"
#include <memory>
#include <queue>
#include <sstream>
#include <numeric>

using namespace michaelcc;

void compiler::check_layout_dependencies(std::shared_ptr<typing::base_type>& type) {
    std::map<std::shared_ptr<typing::base_type>, std::shared_ptr<typing::base_type>> last_seen_parent;

    std::queue<std::shared_ptr<typing::base_type>> type_to_resolve;
    type_to_resolve.push(type);

    while (!type_to_resolve.empty()) {
        std::shared_ptr<typing::base_type> type = type_to_resolve.front();
        if (last_seen_parent.contains(type)) { //check for circular dependencies
            std::ostringstream ss;

            ss << "Circular memory layout dependency detected with type ";
            type->to_string(ss);
            if (m_type_declaration_locations.contains(type)) {
                ss << "(at " << m_type_declaration_locations.at(type).to_string() << ')';
            }

            std::vector<std::shared_ptr<typing::base_type>> path;
            std::shared_ptr<typing::base_type> current = last_seen_parent[type];
            while (current != type) {
                path.push_back(current);
                current = last_seen_parent[current];
            }
            path.push_back(type);
            std::reverse(path.begin(), path.end());
            ss << " in the following path: ";

            for (const auto& path_type : path) {
                path_type->to_string(ss);
                if (m_type_declaration_locations.contains(path_type)) {
                    ss << " (at " << m_type_declaration_locations.at(path_type).to_string() << ')';
                }
                if (path_type != type) {
                    ss << " -> ";
                }
            }
            ss << '.';

            throw compilation_error(ss.str(), m_type_declaration_locations.at(type));
        }
        type_to_resolve.pop();

        const std::vector<std::shared_ptr<typing::base_type>> dependencies = m_layout_dependency_getter(*type);
        for (const auto& dependency : dependencies) {
            type_to_resolve.push(dependency);
            last_seen_parent[dependency] = type;
        }
    }
}

const compiler::type_layout_info compiler::type_layout_calculator::dispatch(const typing::int_type& type) {
    size_t size;
    switch (type.type_class()) {
        case typing::CHAR_INT_CLASS:
            size = 1;
            break;
        case typing::SHORT_INT_CLASS:
            size = m_platform_info.m_short_size;
            break;
        case typing::INT_INT_CLASS:
            size = (type.int_qualifiers() & typing::LONG_INT_QUALIFIER)
                ? m_platform_info.m_long_size
                : m_platform_info.m_int_size;
            break;
        case typing::LONG_INT_CLASS:
            size = m_platform_info.m_long_long_size;
            break;
        default:
            throw std::runtime_error("Invalid int type class");
    }
    return { size, std::min<size_t>(size, m_platform_info.m_max_alignment) };
}

const compiler::type_layout_info compiler::type_layout_calculator::dispatch(const typing::float_type& type) {
    switch (type.type_class()) {
        case typing::FLOAT_FLOAT_CLASS:
            return { 4, std::min<size_t>(4, m_platform_info.m_max_alignment) };
        case typing::DOUBLE_FLOAT_CLASS:
            return { 8, std::min<size_t>(8, m_platform_info.m_max_alignment) };
    }
    throw std::runtime_error("Invalid float type class");
}

const compiler::type_layout_info compiler::type_layout_calculator::dispatch(const typing::struct_type& type) {
    if (m_declared_info.contains(&type)) {
        return m_declared_info.at(&type);
    }

    std::vector<size_t> original_indices(type.fields().size());
    std::iota(original_indices.begin(), original_indices.end(), 0);
    if (m_platform_info.optimize_struct_layout) {
        // Sort by alignment (descending) to minimize padding
        std::sort(original_indices.begin(), original_indices.end(), [this, &type](size_t a, size_t b) {
            const type_layout_info a_layout = (*this)(*type.fields().at(a).field_type.type());
            const type_layout_info b_layout = (*this)(*type.fields().at(b).field_type.type());
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
        const type_layout_info field_layout = (*this)(*field.field_type.type());

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

    auto& mutable_type = const_cast<typing::struct_type&>(type);
    mutable_type.implement_field_offsets(field_offsets);
    m_declared_info.emplace(&mutable_type, type_layout_info{.size=size, .alignment=alignment });
    return m_declared_info.at(&mutable_type);
}

const compiler::type_layout_info compiler::type_layout_calculator::dispatch(const typing::union_type& type) {
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

    max_alignment = std::min<size_t>(max_alignment, m_platform_info.m_max_alignment);
    
    // Pad size to alignment
    size_t size = max_size + (max_alignment - (max_size % max_alignment)) % max_alignment;
    
    m_declared_info.emplace(&type, type_layout_info{.size=size, .alignment=max_alignment });
    return m_declared_info.at(&type);
}

std::shared_ptr<typing::base_type> compiler::type_resolver::dispatch(const ast::type_specifier& type) {
    if (type.type_keywords().size() == 1) {
        switch (type.type_keywords()[0]) {
            case MICHAELCC_TOKEN_VOID:
                return std::make_shared<typing::void_type>();
            case MICHAELCC_TOKEN_FLOAT:
                return std::make_shared<typing::float_type>(typing::FLOAT_FLOAT_CLASS);
            case MICHAELCC_TOKEN_DOUBLE:
                return std::make_shared<typing::float_type>(typing::DOUBLE_FLOAT_CLASS);
            default:
                return resolve_int_type(type);
        }
    }
    return resolve_int_type(type);
}