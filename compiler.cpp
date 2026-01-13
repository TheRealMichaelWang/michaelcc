#include "compiler.hpp"
#include "ast.hpp"
#include "logical.hpp"
#include "typing.hpp"
#include <memory>
#include <queue>
#include <sstream>
#include <numeric>
#include <vector>

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

const compiler::type_layout_info compiler::type_layout_calculator::dispatch(typing::int_type& type) {
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

const compiler::type_layout_info compiler::type_layout_calculator::dispatch(typing::float_type& type) {
    switch (type.type_class()) {
        case typing::FLOAT_FLOAT_CLASS:
            return { 4, std::min<size_t>(4, m_platform_info.m_max_alignment) };
        case typing::DOUBLE_FLOAT_CLASS:
            return { 8, std::min<size_t>(8, m_platform_info.m_max_alignment) };
    }
    throw std::runtime_error("Invalid float type class");
}

const compiler::type_layout_info compiler::type_layout_calculator::dispatch(typing::struct_type& type) {
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

const compiler::type_layout_info compiler::type_layout_calculator::dispatch(typing::union_type& type) {
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

typing::qual_type compiler::type_resolver::resolve_int_type(const ast::type_specifier& type) {
    uint8_t int_qualifiers = typing::NO_INT_QUALIFIER;
    typing::int_class int_class = typing::INT_INT_CLASS;
    
    bool has_char = false, has_short = false, has_int = false;
    int long_count = 0;
    
    for (token_type keyword : type.type_keywords()) {
        switch (keyword) {
            case MICHAELCC_TOKEN_SIGNED:
                int_qualifiers |= typing::SIGNED_INT_QUALIFIER;
                break;
            case MICHAELCC_TOKEN_UNSIGNED:
                int_qualifiers |= typing::UNSIGNED_INT_QUALIFIER;
                break;
            case MICHAELCC_TOKEN_CHAR:
                has_char = true;
                break;
            case MICHAELCC_TOKEN_SHORT:
                has_short = true;
                break;
            case MICHAELCC_TOKEN_INT:
                has_int = true;
                break;
            case MICHAELCC_TOKEN_LONG:
                long_count++;
                break;
            default:
                break;
        }
    }
    
    if (has_char) {
        int_class = typing::CHAR_INT_CLASS;
    }
    else if (has_short) {
        int_class = typing::SHORT_INT_CLASS;
    }
    else if (long_count >= 2) {
        int_class = typing::LONG_INT_CLASS;
    }
    else if (long_count == 1) {
        int_class = typing::INT_INT_CLASS;
        int_qualifiers |= typing::LONG_INT_QUALIFIER;
    }
    else {
        int_class = typing::INT_INT_CLASS;
    }
    
    return typing::qual_type(std::make_shared<typing::int_type>(int_qualifiers, int_class));
}

typing::qual_type compiler::type_resolver::dispatch(const ast::type_specifier& type) {
    if (type.type_keywords().size() == 1) {
        switch (type.type_keywords()[0]) {
            case MICHAELCC_TOKEN_VOID:
                return typing::qual_type(std::make_shared<typing::void_type>());
            case MICHAELCC_TOKEN_FLOAT:
                return typing::qual_type(std::make_shared<typing::float_type>(typing::FLOAT_FLOAT_CLASS));
            case MICHAELCC_TOKEN_DOUBLE:
                return typing::qual_type(std::make_shared<typing::float_type>(typing::DOUBLE_FLOAT_CLASS));
            default:
                return typing::qual_type(resolve_int_type(type));
        }
    }
    return resolve_int_type(type);
}

typing::qual_type compiler::type_resolver::dispatch(const ast::qualified_type& type) {
    auto inner_qual = (*this)(*type.inner_type());
    return typing::qual_type(inner_qual.type(), inner_qual.qualifiers() | type.qualifiers());
}

typing::qual_type compiler::type_resolver::dispatch(const ast::derived_type& type) {
    switch (type.type_kind()) {
        case ast::derived_type::kind::POINTER:
            return typing::qual_type(std::make_shared<typing::pointer_type>(
                (*this)(*type.inner_type()).to_weak()
            ));
        case ast::derived_type::kind::ARRAY:
            return typing::qual_type(std::make_shared<typing::array_type>(
                (*this)(*type.inner_type()).to_weak()
            ));
        default:
            throw std::runtime_error("Invalid derived type kind");
    }
}

typing::qual_type compiler::type_resolver::dispatch(const ast::function_type& type) {
    std::vector<typing::qual_type> param_types;
    param_types.reserve(type.parameters().size());
    for (const auto& param : type.parameters()) {
        param_types.push_back((*this)(*param.param_type));
    }
    return typing::qual_type(std::make_shared<typing::function_pointer_type>(
        (*this)(*type.return_type()), param_types
    ));
}

typing::qual_type compiler::type_resolver::dispatch(const ast::struct_declaration& type) {
    auto struct_type = m_compiler.m_translation_unit.lookup_struct(type.struct_name().value());
    if (struct_type != nullptr) {
        return typing::qual_type(struct_type);
    }

    std::vector<typing::member> fields;
    fields.reserve(type.fields().size());
    for (const auto& field : type.fields()) {
        fields.emplace_back(typing::member{
            field.identifier(),
            (*this)(*field.type())
        });
    }

    auto my_struct_type = std::make_shared<typing::struct_type>(type.struct_name().value(), std::move(fields));
    m_compiler.m_type_layout_calculator(*my_struct_type); //implement layout

    return typing::qual_type();
}

typing::qual_type compiler::type_resolver::dispatch(const ast::union_declaration& type) {
    auto union_type = m_compiler.m_translation_unit.lookup_union(type.union_name().value());
    if (union_type != nullptr) {
        return typing::qual_type::weak(union_type);
    }

    std::vector<typing::member> members;
    members.reserve(type.members().size());
    for (const auto& member : type.members()) {
        members.emplace_back(typing::member{
            member.member_name,
            (*this)(*member.member_type),
            0
        });
    }
    return typing::qual_type(std::make_shared<typing::union_type>(type.union_name().value(), std::move(members)));
}

typing::qual_type compiler::type_resolver::dispatch(const ast::enum_declaration& type) {
    auto enum_type = m_compiler.m_translation_unit.lookup_enum(type.enum_name().value());
    if (enum_type != nullptr) {
        return typing::qual_type::weak(enum_type);
    }
    std::vector<typing::enum_type::enumerator> enumerators;
    enumerators.reserve(type.enumerators().size());

    int64_t max_value = 0;
    for (const auto& enumerator : type.enumerators()) {
        enumerators.emplace_back(typing::enum_type::enumerator{
            enumerator.name,
            enumerator.value.value_or(max_value)
        });
        max_value = std::max(max_value, enumerator.value.value_or(max_value));
        max_value++;
    }
    return typing::qual_type(std::make_shared<typing::enum_type>(type.enum_name().value(), std::move(enumerators)));
}

void compiler::type_resolver::handle_default(const ast::ast_element& node) {
    std::ostringstream ss;
    ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid type.";
    throw panic(ss.str(), node.location());
}

std::unique_ptr<logical_ir::expression> compiler::address_of_compiler::dispatch(const ast::variable_reference& node) {
    auto symbol = m_compiler.m_symbol_explorer.lookup(node.identifier());
    if (symbol == nullptr) {
        std::ostringstream ss;
        ss << "Symbol \"" << node.identifier() << "\" not found.";
        throw panic(ss.str(), node.location());
    }
    
    std::shared_ptr<logical_ir::variable> variable = std::dynamic_pointer_cast<logical_ir::variable>(symbol);
    if (variable) {
        return std::make_unique<logical_ir::address_of>(std::move(variable));
    }

    std::shared_ptr<logical_ir::function_definition> function_definition = std::dynamic_pointer_cast<logical_ir::function_definition>(symbol);
    if (function_definition) {
        return std::make_unique<logical_ir::function_reference>(std::move(function_definition));
    }

    std::ostringstream ss;
    ss << "Symbol \"" << node.identifier() << "\" is not a variable, array index, or member access.";
    throw panic(ss.str(), node.location());
}

std::unique_ptr<logical_ir::expression> compiler::address_of_compiler::dispatch(const ast::get_index& node) {
    std::unique_ptr<logical_ir::array_index> array_index = dynamic_unique_cast<logical_ir::array_index>(m_compiler.compile_expression(node));

    if (array_index == nullptr) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not an array index.";
        throw panic(ss.str(), node.location());
    }
    return std::make_unique<logical_ir::address_of>(std::move(array_index));
}

std::unique_ptr<logical_ir::expression> compiler::address_of_compiler::dispatch(const ast::get_property& node) {
    std::unique_ptr<logical_ir::member_access> member_access = dynamic_unique_cast<logical_ir::member_access>(m_compiler.compile_expression(node));
    if (member_access == nullptr) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not a member access.";
        throw panic(ss.str(), node.location());
    }
    return std::make_unique<logical_ir::address_of>(std::move(member_access));
}

std::unique_ptr<logical_ir::expression> compiler::address_of_compiler::dispatch(const ast::dereference_operator& node) {
    std::unique_ptr<logical_ir::dereference> dereference = dynamic_unique_cast<logical_ir::dereference>(m_compiler.compile_expression(node));
    if (dereference == nullptr) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not a dereference operator.";
        throw panic(ss.str(), node.location());
    }

    std::unique_ptr<logical_ir::expression> operand = dereference->release_operand();
    return operand;
}

void compiler::address_of_compiler::handle_default(const ast::ast_element& node) {
    std::ostringstream ss;
    ss << "Cannot get the address of \"" << ast::to_c_string(node) << "\".";
    throw panic(ss.str(), node.location());
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::int_literal& node) {
    if (m_target_type && typeid(m_target_type->type()) == typeid(typing::int_type)) {
        return std::make_unique<logical_ir::integer_constant>(node.value(), typing::qual_type(m_target_type.value()));
    };
    return std::make_unique<logical_ir::integer_constant>(node.value(), 
    typing::qual_type::owning(std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS))
    );
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::float_literal& node) {
    if (m_target_type && typeid(m_target_type->type()) == typeid(typing::float_type)) {
        return std::make_unique<logical_ir::floating_constant>(node.value(), typing::qual_type(m_target_type.value()));
    };
    return std::make_unique<logical_ir::floating_constant>(node.value(), 
    typing::qual_type::owning(std::make_shared<typing::float_type>(typing::FLOAT_FLOAT_CLASS))
    );
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::double_literal& node) {
    if (m_target_type && typeid(m_target_type->type()) == typeid(typing::float_type)) {
        return std::make_unique<logical_ir::floating_constant>(node.value(), typing::qual_type(m_target_type.value()));
    };
    return std::make_unique<logical_ir::floating_constant>(node.value(), 
    typing::qual_type::owning(std::make_shared<typing::float_type>(typing::FLOAT_FLOAT_CLASS))
    );
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::string_literal& node) {
    size_t index = m_compiler.m_translation_unit.add_string(std::string(node.value()));
    return std::make_unique<logical_ir::string_constant>(index);
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::variable_reference& node) {
    auto symbol = m_compiler.m_symbol_explorer.lookup(node.identifier());

    if (symbol == nullptr) {
        std::ostringstream ss;
        ss << "Symbol \"" << node.identifier() << "\" not found.";
        throw panic(ss.str(), node.location());
    }

    std::shared_ptr<logical_ir::variable> variable = std::dynamic_pointer_cast<logical_ir::variable>(symbol);
    if (variable) {
        return std::make_unique<logical_ir::variable_reference>(std::move(variable));
    }

    std::shared_ptr<logical_ir::function_definition> function = std::dynamic_pointer_cast<logical_ir::function_definition>(symbol);
    if (function) {
        return std::make_unique<logical_ir::function_reference>(std::move(function));
    }

    std::ostringstream ss;
    ss << "Symbol \"" << node.identifier() << "\" is not a variable or function that can be captured as an expression.";
    throw panic(ss.str(), node.location());
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::get_index& node) {
    std::unique_ptr<logical_ir::expression> pointer = m_compiler.compile_expression(*node.pointer());
    std::unique_ptr<logical_ir::expression> index = m_compiler.compile_expression(*node.index(), std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS));

    if (!m_compiler.is_indexable_type(pointer->get_type())) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not an indexable type.";
        throw panic(ss.str(), node.location());
    }

    if (!m_compiler.is_index_type(index->get_type())) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not an index type.";
        throw panic(ss.str(), node.location());
    }

    return std::make_unique<logical_ir::array_index>(std::move(pointer), std::move(index));
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::get_property& node) {
    std::unique_ptr<logical_ir::expression> base = m_compiler.compile_expression(*node.struct_expr());
    typing::qual_type base_type = base->get_type();

    std::shared_ptr<typing::struct_type> struct_type = std::dynamic_pointer_cast<typing::struct_type>(base_type.type());
    if (struct_type) {
        std::vector<typing::member> fields = struct_type->fields();
        auto field = std::find_if(fields.begin(), fields.end(), [name = node.property_name()](const typing::member& member) {
            return member.name == name;
        });
        if (field == fields.end()) {
            std::ostringstream ss;
            ss << "Struct \"" << struct_type->name().value() << "\" does not have a member named \"" << node.property_name() << "\".";
            throw panic(ss.str(), node.location());
        }

        return std::make_unique<logical_ir::member_access>(std::move(base), *field);
    }

    std::shared_ptr<typing::union_type> union_type = std::dynamic_pointer_cast<typing::union_type>(base_type.type());
    if (union_type) {
        std::vector<typing::member> members = union_type->members();
        auto member = std::find_if(members.begin(), members.end(), [name = node.property_name()](const typing::member& member) {
            return member.name == name;
        });
        if (member == members.end()) {
            std::ostringstream ss;
            ss << "Union \"" << union_type->name().value() << "\" does not have a member named \"" << node.property_name() << "\".";
            throw panic(ss.str(), node.location());
        }
        return std::make_unique<logical_ir::member_access>(std::move(base), *member);
    }

    std::ostringstream ss;
    ss << "Expression \"" << ast::to_c_string(node) << "\" is not a struct or union.";
    throw panic(ss.str(), node.location());
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::dereference_operator& node) {
    std::unique_ptr<logical_ir::expression> pointer = m_compiler.compile_expression(*node.pointer());
    if (typeid(pointer->get_type().type()) != typeid(typing::pointer_type)) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not a pointer.";
        throw panic(ss.str(), node.location());
    }
    return std::make_unique<logical_ir::dereference>(std::move(pointer));
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::get_reference& node) {
    return m_compiler.m_address_of_compiler(*node.item());
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::arithmetic_operator& node) {
    std::unique_ptr<logical_ir::expression> left = m_compiler.compile_expression(*node.left());
    std::unique_ptr<logical_ir::expression> right = m_compiler.compile_expression(*node.right());

    if (m_compiler.is_numeric_type(left->get_type()) && m_compiler.is_numeric_type(right->get_type())) {
        if (typeid(left->get_type().type()) == typeid(typing::int_type) && typeid(right->get_type().type()) == typeid(typing::int_type)) {
            
        }
    }
}

void compiler::expression_compiler::handle_default(const ast::ast_element& node) {
    std::ostringstream ss;
    ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid expression.";
    throw panic(ss.str(), node.location());
}