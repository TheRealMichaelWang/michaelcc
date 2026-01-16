#include "compiler.hpp"
#include "ast.hpp"
#include "logical.hpp"
#include "tokens.hpp"
#include "typing.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <sstream>
#include <numeric>
#include <vector>
#include <iostream>

using namespace michaelcc;

void compiler::check_layout_dependencies(const std::shared_ptr<typing::base_type>& type) {
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
                m_compiler.resolve_type(*type.inner_type()).to_weak()
            ));
        case ast::derived_type::kind::ARRAY:
            if (type.array_size()) {
                if (!m_allow_vla) {
                    std::ostringstream ss;
                    ss << "Variable length array is not allowed in this context.";
                    throw panic(ss.str(), type.location());
                }
                m_vla_dimensions.push_back(m_compiler.compile_expression(*type.array_size().value()));
            }
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
        param_types.push_back(m_compiler.resolve_type(*param.param_type));
    }
    return typing::qual_type(std::make_shared<typing::function_pointer_type>(
        m_compiler.resolve_type(*type.return_type()), param_types
    ));
}

typing::qual_type compiler::type_resolver::dispatch(const ast::struct_declaration& type) {
    if (type.struct_name().has_value()) {
        auto struct_type = m_compiler.m_translation_unit.lookup_struct(type.struct_name().value());
        if (!struct_type) {
            std::ostringstream ss;
            ss << "Struct \"" << type.struct_name().value() << "\" not found.";
            throw panic(ss.str(), type.location());
        }
        return typing::qual_type::weak(struct_type);
    }

    std::vector<typing::member> fields;
    fields.reserve(type.fields().size());
    for (const auto& field : type.fields()) {
        fields.emplace_back(typing::member{
            field.identifier(),
            m_compiler.resolve_type(*field.type())
        });
    }

    auto my_struct_type = std::make_shared<typing::struct_type>(std::nullopt, std::move(fields));
    m_compiler.m_type_layout_calculator(*my_struct_type); //implement layout

    return typing::qual_type::owning(my_struct_type);
}

typing::qual_type compiler::type_resolver::dispatch(const ast::union_declaration& type) {
    if (type.union_name().has_value()) {
        auto union_type = m_compiler.m_translation_unit.lookup_union(type.union_name().value());
        if (!union_type) {
            std::ostringstream ss;
            ss << "Union \"" << type.union_name().value() << "\" not found.";
            throw panic(ss.str(), type.location());
        }
        return typing::qual_type::weak(union_type);
    }

    std::vector<typing::member> members;
    members.reserve(type.members().size());
    for (const auto& member : type.members()) {
        members.emplace_back(typing::member{
            member.member_name,
            m_compiler.resolve_type(*member.member_type),
            0
        });
    }
    auto my_union_type = std::make_shared<typing::union_type>(std::nullopt, std::move(members));
    m_compiler.m_type_layout_calculator(*my_union_type); //implement layout
    
    return typing::qual_type::owning(my_union_type);
}

typing::qual_type compiler::type_resolver::dispatch(const ast::enum_declaration& type) {
    if (type.enum_name().has_value()) {
        auto enum_type = m_compiler.m_translation_unit.lookup_enum(type.enum_name().value());
        if (!enum_type) {
            std::ostringstream ss;
            ss << "Enum \"" << type.enum_name().value() << "\" not found.";
            throw panic(ss.str(), type.location());
        }
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
    return typing::qual_type::owning(std::make_shared<typing::enum_type>(std::nullopt, std::move(enumerators)));
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
    if (m_target_type && m_target_type->is_same_type<typing::int_type>()) {
        return std::make_unique<logical_ir::integer_constant>(node.value(), typing::qual_type(m_target_type.value()));
    };
    return std::make_unique<logical_ir::integer_constant>(node.value(), 
    typing::qual_type::owning(std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS))
    );
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::float_literal& node) {
    if (m_target_type && m_target_type->is_same_type<typing::float_type>()) {
        return std::make_unique<logical_ir::floating_constant>(node.value(), typing::qual_type(m_target_type.value()));
    };
    return std::make_unique<logical_ir::floating_constant>(node.value(), 
    typing::qual_type::owning(std::make_shared<typing::float_type>(typing::FLOAT_FLOAT_CLASS))
    );
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::double_literal& node) {
    if (m_target_type && m_target_type->is_same_type<typing::float_type>()) {
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
    std::unique_ptr<logical_ir::expression> index = m_compiler.compile_expression(*node.index(), std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS), true);

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
        const std::vector<typing::member>& fields = struct_type->fields();
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
        const std::vector<typing::member>& members = union_type->members();
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

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::set_operator& node) {
    const ast::variable_reference* variable_reference = dynamic_cast<const ast::variable_reference*>(node.destination().get());
    if (variable_reference) {
        auto symbol = m_compiler.m_symbol_explorer.lookup(variable_reference->identifier());
        if (symbol == nullptr) {
            std::ostringstream ss;
            ss << "Symbol \"" << variable_reference->identifier() << "\" not found.";
            throw panic(ss.str(), node.location());
        }
        std::shared_ptr<logical_ir::variable> variable = std::dynamic_pointer_cast<logical_ir::variable>(symbol);
        if (!variable) {
            std::ostringstream ss;
            ss << "Symbol \"" << variable_reference->identifier() << "\" is not a variable.";
            throw panic(ss.str(), node.location());
        }
        if (variable->get_type().is_const()) {
            std::ostringstream ss;
            ss << "Variable \"" << variable_reference->identifier() << "\" is const and cannot be assigned to.";
            throw panic(ss.str(), node.location());
        }

        return std::make_unique<logical_ir::set_variable>(std::move(variable), m_compiler.compile_expression(*node.value(), variable->get_type()));
    }

    std::unique_ptr<logical_ir::expression> destination = m_compiler.m_address_of_compiler(*node.destination());
    std::shared_ptr<typing::pointer_type> pointer_type = std::static_pointer_cast<typing::pointer_type>(destination->get_type().type());
    if (destination->get_type().is_const()) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is const and cannot be assigned to.";
        throw panic(ss.str(), node.location());
    }
    return std::make_unique<logical_ir::set_address>(std::move(destination), m_compiler.compile_expression(*node.value(), pointer_type->pointee_type()));
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::dereference_operator& node) {
    std::unique_ptr<logical_ir::expression> pointer = m_compiler.compile_expression(*node.pointer());
    if (!pointer->get_type().is_same_type<typing::pointer_type>()) {
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
    if (node.operation() == MICHAELCC_TOKEN_INCREMENT_BY || node.operation() == MICHAELCC_TOKEN_DECREMENT_BY) {
        std::unique_ptr<logical_ir::expression> destination = m_compiler.m_address_of_compiler(*node.left());
        if (destination == nullptr) {
            std::ostringstream ss;
            ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid destination for increment or decrement.";
            throw panic(ss.str(), node.location());
        }
        return std::make_unique<logical_ir::var_increment_operator>(std::move(destination), m_compiler.compile_expression(*node.right()));
    }

    std::unique_ptr<logical_ir::expression> left = m_compiler.compile_expression(*node.left());
    std::unique_ptr<logical_ir::expression> right = m_compiler.compile_expression(*node.right());

    std::optional<typing::qual_type> result = m_compiler.arbitrate_types(left->get_type(), right->get_type(), true);
    if (!result) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid arithmetic operation.";
        ss << "Cannot arbitrate types \"" << left->get_type().to_string() << "\" and \"" << right->get_type().to_string() << "\".";
        throw panic(ss.str(), node.location());
    }
    return std::make_unique<logical_ir::arithmetic_operator>(
        node.operation(),
        std::make_unique<logical_ir::type_cast>(std::move(left), typing::qual_type(result.value())), 
        std::make_unique<logical_ir::type_cast>(std::move(right), typing::qual_type(result.value())), 
        std::move(result.value())
    );
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::unary_operation& node) {
    switch (node.get_operator()) {
        case MICHAELCC_TOKEN_MINUS: {
            auto operand = m_compiler.compile_expression(*node.operand(), m_target_type, true);
            if (!m_compiler.is_numeric_type(operand->get_type())) {
                std::ostringstream ss;
                ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid numeric type.";
                throw panic(ss.str(), node.location());
            }
            return std::make_unique<logical_ir::unary_operation>(
                node.get_operator(), 
                std::move(operand)
            );
        }
        case MICHAELCC_TOKEN_NOT: {
            auto operand = m_compiler.compile_expression(*node.operand(), std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS), true);
            if (!operand->get_type().is_same_type<typing::int_type>()) {
                std::ostringstream ss;
                ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid conditional type.";
                throw panic(ss.str(), node.location());
            }
            return std::make_unique<logical_ir::unary_operation>(node.get_operator(), std::move(operand));
        }
        default:
            std::ostringstream ss;
            ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid unary operation since " << token_to_str(node.get_operator()) << " is not a valid unary operator.";
            throw panic(ss.str(), node.location());
    }
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::conditional_expression& node) {
    std::unique_ptr<logical_ir::expression> condition = m_compiler.compile_expression(*node.condition());
    std::unique_ptr<logical_ir::expression> true_expr = m_compiler.compile_expression(*node.true_expr());
    std::unique_ptr<logical_ir::expression> false_expr = m_compiler.compile_expression(*node.false_expr());

    if (!condition->get_type().is_same_type<typing::int_type>()) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid condition.";
        throw panic(ss.str(), node.location());
    }

    std::optional<typing::qual_type> result = m_compiler.arbitrate_types(true_expr->get_type(), false_expr->get_type());
    if (!result) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid conditional expression.";
        ss << "Cannot arbitrate types \"" << true_expr->get_type().to_string() << "\" and \"" << false_expr->get_type().to_string() << "\".";
        throw panic(ss.str(), node.location());
    }
    return std::make_unique<logical_ir::conditional_expression>(
        std::move(condition),
        std::make_unique<logical_ir::type_cast>(std::move(true_expr), typing::qual_type(result.value())),
        std::make_unique<logical_ir::type_cast>(std::move(false_expr), typing::qual_type(result.value())),
        std::move(result.value())
    );
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::function_call& node) {
    std::vector<std::unique_ptr<logical_ir::expression>> arguments;
    arguments.reserve(node.arguments().size());
    for (const auto& argument : node.arguments()) {
        arguments.push_back(m_compiler.compile_expression(*argument));
    }

    ast::variable_reference* variable_reference = dynamic_cast<ast::variable_reference*>(node.callee().get());
    if (variable_reference) {
        auto symbol = m_compiler.m_symbol_explorer.lookup(variable_reference->identifier());
        if (symbol == nullptr) {
            std::ostringstream ss;
            ss << "Symbol \"" << variable_reference->identifier() << "\" not found.";
            throw panic(ss.str(), node.location());
        }
        std::shared_ptr<logical_ir::function_definition> function = std::dynamic_pointer_cast<logical_ir::function_definition>(symbol);
        if (function) {
            if (!function->is_implemented()) {
                std::ostringstream ss;
                ss << "Function \"" << function->name() << "\" is not implemented and thus cannot be called.";
                throw panic(ss.str(), node.location());
            }

            if (function->parameters().size() != arguments.size()) {
                std::ostringstream ss;
                ss << "Function \"" << function->name() << "\" expects " << function->parameters().size() << " arguments, but " << arguments.size() << " were provided.";
                throw panic(ss.str(), node.location());
            }
            for (size_t i = 0; i < function->parameters().size(); i++) {
                if (!arguments[i]->get_type().is_assignable_from(function->parameters()[i]->get_type())) {
                    std::ostringstream ss;
                    ss << "Argument " << i << " is not the same type as the parameter " << function->parameters()[i]->name() << ". ";
                    ss << "Expected " << function->parameters()[i]->get_type().to_string() << ", but got " << arguments[i]->get_type().to_string() << ".";
                    throw panic(ss.str(), node.location());
                }
            }
            return std::make_unique<logical_ir::function_call>(std::make_shared<logical_ir::function_reference>(std::move(function)), std::move(arguments));
        }
    }

    std::unique_ptr<logical_ir::expression> callee = m_compiler.compile_expression(*node.callee());
    std::shared_ptr<typing::function_pointer_type> function_pointer = std::dynamic_pointer_cast<typing::function_pointer_type>(callee->get_type().type());
    if (function_pointer == nullptr) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not a function pointer and thus cannot be called.";
        throw panic(ss.str(), node.location());
    }

    if (function_pointer->parameter_types().size() != arguments.size()) {
        std::ostringstream ss;
        ss << "Function pointer expects " << function_pointer->parameter_types().size() << " arguments, but " << arguments.size() << " were provided.";
        throw panic(ss.str(), node.location());
    }
    for (size_t i = 0; i < function_pointer->parameter_types().size(); i++) {
        if (!arguments[i]->get_type().is_assignable_from(function_pointer->parameter_types()[i])) {
            std::ostringstream ss;
            ss << "Argument " << i << " is not the same type as the parameter " << function_pointer->parameter_types()[i].to_string() << ". ";
            ss << "Expected " << function_pointer->parameter_types()[i].to_string() << ", but got " << arguments[i]->get_type().to_string() << ".";
            throw panic(ss.str(), node.location());
        }
    }
    return std::make_unique<logical_ir::function_call>(std::move(callee), std::move(arguments));
}

std::unique_ptr<logical_ir::expression> compiler::expression_compiler::dispatch(const ast::initializer_list_expression& node) {
    std::shared_ptr<typing::struct_type> struct_type = std::dynamic_pointer_cast<typing::struct_type>(m_target_type->type());
    if (struct_type) {
        std::vector<logical_ir::struct_initializer::member_initializer> member_initializers;
        member_initializers.reserve(node.initializers().size());

        const std::vector<typing::member>& members = struct_type->fields();
        if (members.size() != node.initializers().size()) {
            std::ostringstream ss;
            ss << "Struct \"" << struct_type->name().value() << "\" expects " << members.size() << " initializers, but " << node.initializers().size() << " were provided.";
            throw panic(ss.str(), node.location());
        }
        for (size_t i = 0; i < members.size(); i++) {
            member_initializers.emplace_back(logical_ir::struct_initializer::member_initializer{
                members[i].name,
                m_compiler.compile_expression(*node.initializers()[i], members[i].member_type)
            });
        }
        return std::make_unique<logical_ir::struct_initializer>(std::move(member_initializers), std::move(struct_type));
    }

    std::shared_ptr<typing::union_type> union_type = std::dynamic_pointer_cast<typing::union_type>(m_target_type->type());
    if (union_type) {
        if (node.initializers().size() != 1) {
            std::ostringstream ss;
            ss << "Union \"" << union_type->name().value() << "\" expects 1 initializer, but " << node.initializers().size() << " were provided.";
            throw panic(ss.str(), node.location());
        }

        std::unique_ptr<logical_ir::expression> initializer = m_compiler.compile_expression(*node.initializers()[0]);
        std::optional<typing::qual_type> target_type = std::nullopt;

        for (const auto& member : union_type->members()) {
            if (member.member_type.is_assignable_from(initializer->get_type())) {
                target_type = member.member_type;
                break;
            }
        }
        if (!target_type) {
            std::ostringstream ss;
            ss << "Union \"" << union_type->name().value() << "\" does not have a member that is assignable from " << initializer->get_type().to_string() << ".";
            throw panic(ss.str(), node.location());
        }
        return std::make_unique<logical_ir::union_initializer>(std::move(initializer), std::move(union_type), std::move(target_type.value()));
    }
    
    std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(m_target_type->type());
    if (array_type) {
        std::vector<std::unique_ptr<logical_ir::expression>> initializers;
        initializers.reserve(node.initializers().size());
        for (const auto& initializer : node.initializers()) {
            initializers.push_back(m_compiler.compile_expression(*initializer, array_type->element_type()));
        }
        return std::make_unique<logical_ir::array_initializer>(std::move(initializers), typing::qual_type(array_type->element_type()));
    }

    std::ostringstream ss;
    ss << "Expression \"" << ast::to_c_string(node) << "\" is not a valid initializer list expression.";
    throw panic(ss.str(), node.location());
}

void compiler::expression_compiler::handle_default(const ast::ast_element& node) {
    std::ostringstream ss;
    ss << "\"" << ast::to_c_string(node) << "\" is not a valid expression.";
    throw panic(ss.str(), node.location());
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::context_block& node) {
    std::shared_ptr<logical_ir::control_block> control_block = std::make_shared<logical_ir::control_block>();
    m_compiler.m_symbol_explorer.visit(control_block);

    std::vector<std::unique_ptr<logical_ir::statement>> statements;
    statements.reserve(node.statements().size());
    for (const auto& statement : node.statements()) {
        statements.push_back((*this)(*statement));
    }

    m_compiler.m_symbol_explorer.exit();
    control_block->implement(std::move(statements));
    return std::make_unique<logical_ir::statement_block>(std::move(control_block));
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::for_loop& node) {
    std::shared_ptr<logical_ir::control_block> outer_control_block = std::make_shared<logical_ir::control_block>();
    m_compiler.m_symbol_explorer.visit(outer_control_block);

    std::vector<std::unique_ptr<logical_ir::statement>> outer_statements;
    outer_statements.emplace_back((*this)(*node.initial_statement()));
    std::unique_ptr<logical_ir::expression> condition = m_compiler.compile_expression(*node.condition(), std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS), true);
    if (!condition->get_type().is_same_type<typing::int_type>()) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(*node.condition()) << "\" is not a valid condition.";
        throw panic(ss.str(), node.condition()->location());
    }

    m_current_loop_depth++;
    std::shared_ptr<logical_ir::control_block> inner_control_block = std::make_shared<logical_ir::control_block>();
    m_compiler.m_symbol_explorer.visit(inner_control_block);
    std::vector<std::unique_ptr<logical_ir::statement>> inner_statements;
    inner_statements.reserve(node.body().statements().size());
    for (const auto& statement : node.body().statements()) {
        inner_statements.emplace_back((*this)(*statement));
    }
    m_compiler.m_symbol_explorer.exit();
    m_current_loop_depth--;
    inner_statements.emplace_back((*this)(*node.increment_statement()));
    inner_control_block->implement(std::move(inner_statements));

    outer_statements.emplace_back(std::make_unique<logical_ir::loop_statement>(std::move(inner_control_block), std::move(condition), true));
    outer_control_block->implement(std::move(outer_statements));
    return std::make_unique<logical_ir::statement_block>(std::move(outer_control_block));
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::do_block& node) {
    std::shared_ptr<logical_ir::control_block> control_block = std::make_shared<logical_ir::control_block>();
    m_compiler.m_symbol_explorer.visit(control_block);

    m_current_loop_depth++;
    std::vector<std::unique_ptr<logical_ir::statement>> statements;
    statements.reserve(node.body().statements().size());
    for (const auto& statement : node.body().statements()) {
        statements.emplace_back((*this)(*statement));
    }
    control_block->implement(std::move(statements));

    std::unique_ptr<logical_ir::expression> condition = m_compiler.compile_expression(*node.condition(), std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS));
    if (!condition->get_type().is_same_type<typing::int_type>()) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(*node.condition()) << "\" is not a valid condition.";
        throw panic(ss.str(), node.condition()->location());
    }

    m_current_loop_depth--;
    m_compiler.m_symbol_explorer.exit();
    return std::make_unique<logical_ir::loop_statement>(std::move(control_block), std::move(condition), false);
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::while_block& node) {
    std::unique_ptr<logical_ir::expression> condition = m_compiler.compile_expression(*node.condition(), std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS));
    if (!condition->get_type().is_same_type<typing::int_type>()) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(*node.condition()) << "\" is not a valid condition.";
        throw panic(ss.str(), node.condition()->location());
    }

    std::shared_ptr<logical_ir::control_block> control_block = std::make_shared<logical_ir::control_block>();
    m_compiler.m_symbol_explorer.visit(control_block);

    m_current_loop_depth++;
    std::vector<std::unique_ptr<logical_ir::statement>> statements;
    statements.reserve(node.body().statements().size());
    for (const auto& statement : node.body().statements()) {
        statements.emplace_back((*this)(*statement));
    }
    control_block->implement(std::move(statements));

    m_current_loop_depth--;
    m_compiler.m_symbol_explorer.exit();
    return std::make_unique<logical_ir::loop_statement>(std::move(control_block), std::move(condition), true);
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::if_block& node) {
    std::unique_ptr<logical_ir::expression> condition = m_compiler.compile_expression(*node.condition(), std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS));
    if (!condition->get_type().is_same_type<typing::int_type>()) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(*node.condition()) << "\" is not a valid condition.";
        throw panic(ss.str(), node.condition()->location());
    }

    std::shared_ptr<logical_ir::control_block> then_control_block = std::make_shared<logical_ir::control_block>();
    m_compiler.m_symbol_explorer.visit(then_control_block);

    std::vector<std::unique_ptr<logical_ir::statement>> then_statements;
    then_statements.reserve(node.body().statements().size());
    for (const auto& statement : node.body().statements()) {
        then_statements.emplace_back((*this)(*statement));
    }
    then_control_block->implement(std::move(then_statements));

    m_compiler.m_symbol_explorer.exit();
    return std::make_unique<logical_ir::if_statement>(std::move(condition), std::move(then_control_block), nullptr);
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::if_else_block& node) {
    std::unique_ptr<logical_ir::expression> condition = m_compiler.compile_expression(*node.condition(), std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS));
    if (!condition->get_type().is_same_type<typing::int_type>()) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(*node.condition()) << "\" is not a valid condition.";
        throw panic(ss.str(), node.condition()->location());
    }

    std::shared_ptr<logical_ir::control_block> then_control_block = std::make_shared<logical_ir::control_block>();
    m_compiler.m_symbol_explorer.visit(then_control_block);

    std::vector<std::unique_ptr<logical_ir::statement>> then_statements;
    then_statements.reserve(node.true_body().statements().size());
    for (const auto& statement : node.true_body().statements()) {
        then_statements.emplace_back((*this)(*statement));
    }
    then_control_block->implement(std::move(then_statements));

    m_compiler.m_symbol_explorer.exit();

    std::shared_ptr<logical_ir::control_block> else_control_block = std::make_shared<logical_ir::control_block>();
    m_compiler.m_symbol_explorer.visit(else_control_block);

    std::vector<std::unique_ptr<logical_ir::statement>> else_statements;
    else_statements.reserve(node.false_body().statements().size());
    for (const auto& statement : node.false_body().statements()) {
        else_statements.emplace_back((*this)(*statement));
    }
    else_control_block->implement(std::move(else_statements));

    m_compiler.m_symbol_explorer.exit();
    return std::make_unique<logical_ir::if_statement>(std::move(condition), std::move(then_control_block), std::move(else_control_block));
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::return_statement& node) {
    std::shared_ptr<logical_ir::function_definition> function = m_compiler.m_symbol_explorer.is_in_context<logical_ir::function_definition>();
    if (function == nullptr) {
        std::ostringstream ss;
        ss << "Return statement is not in a function context.";
        throw panic(ss.str(), node.location());
    }
    
    return std::make_unique<logical_ir::return_statement>(
        m_compiler.compile_expression(*node.value(), function->return_type()), 
        function
    );
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::break_statement& node) {
    if (node.depth() > m_current_loop_depth) {
        std::ostringstream ss;
        ss << "Break statement depth is greater than the current loop depth.";
        throw panic(ss.str(), node.location());
    }
    return std::make_unique<logical_ir::break_statement>(node.depth());
}

std::unique_ptr<logical_ir::statement> compiler::statement_compiler::dispatch(const ast::continue_statement& node) {
    if (node.depth() > m_current_loop_depth) {
        std::ostringstream ss;
        ss << "Continue statement depth is greater than the current loop depth.";
        throw panic(ss.str(), node.location());
    }
    return std::make_unique<logical_ir::continue_statement>(node.depth());
}

void compiler::statement_compiler::handle_default(const ast::ast_element& node) {
    std::ostringstream ss;
    ss << "\"" << ast::to_c_string(node) << "\" is not a valid statement.";
    throw panic(ss.str(), node.location());
}

std::optional<typing::qual_type> compiler::arbitrate_types(const typing::qual_type& left, const typing::qual_type& right, bool arbitrate_numeric) const noexcept {
    if (left == right) {
        if (arbitrate_numeric && !left.is_same_type<typing::int_type>() && !left.is_same_type<typing::float_type>()) {
            return std::nullopt;
        }
        return left;
    }

    if (is_numeric_type(left) && is_numeric_type(right)) {
        if (left.is_same_type<typing::int_type>() && right.is_same_type<typing::int_type>()) {
            const typing::int_type& left_int = static_cast<const typing::int_type&>(*left.type());
            const typing::int_type& right_int = static_cast<const typing::int_type&>(*right.type());
            
            uint8_t max_info_qualifiers = right_int.int_qualifiers() | left_int.int_qualifiers();
            if (max_info_qualifiers & typing::SIGNED_INT_QUALIFIER) { //remove signed qualifier
                max_info_qualifiers &= ~typing::SIGNED_INT_QUALIFIER;
            }
            typing::int_class max_info_class = std::max(left_int.type_class(), right_int.type_class());
            return typing::qual_type(std::make_shared<typing::int_type>(max_info_qualifiers, max_info_class));
        }
        if (left.is_same_type<typing::float_type>() && right.is_same_type<typing::float_type>()) {
            const typing::float_type& left_float = static_cast<const typing::float_type&>(*left.type());
            const typing::float_type& right_float = static_cast<const typing::float_type&>(*right.type());
            typing::float_class max_info_class = std::max(left_float.type_class(), right_float.type_class());
            return typing::qual_type(std::make_shared<typing::float_type>(max_info_class));
        }
        if (left.is_same_type<typing::float_type>() && right.is_same_type<typing::int_type>()) {
            return left;
        }
        if (left.is_same_type<typing::int_type>() && right.is_same_type<typing::float_type>()) {
            return right;
        }
    }
    if (left.is_same_type<typing::pointer_type>() && right.is_same_type<typing::int_type>()) {
        return left;
    }
    if (left.is_same_type<typing::int_type>() && right.is_same_type<typing::pointer_type>()) {
        return right;
    }

    return std::nullopt;
}

typing::qual_type compiler::resolve_type(const ast::ast_element& node, bool allow_vla) {
    type_resolver type_resolver(*this, allow_vla);
    return type_resolver(node);
}

std::unique_ptr<logical_ir::expression> compiler::compile_expression(const ast::ast_element& node, std::optional<typing::qual_type> target_type, bool is_type_hint) {
    expression_compiler expression_compiler(*this, target_type);
    std::unique_ptr<logical_ir::expression> expression = expression_compiler(node);

    if (target_type && !target_type->is_assignable_from(expression->get_type()) && !is_type_hint) {
        std::ostringstream ss;
        ss << "Expression \"" << ast::to_c_string(node) << "\" is not the same type as the target type.";
        throw panic(ss.str(), node.location());
    }
    return expression;
}

logical_ir::variable_declaration compiler::compile_variable_declaration(const ast::variable_declaration& node, bool is_global) {
    type_resolver var_type_resolver(*this, true);
    typing::qual_type type = var_type_resolver(*node.type());

    std::shared_ptr<logical_ir::variable> variable = std::make_shared<logical_ir::variable>(
        std::string(node.identifier()),
        node.qualifiers(),
        typing::qual_type(type),
        is_global,
        m_symbol_explorer.current_context()
    );
    if(!m_symbol_explorer.add(variable)) {
        std::ostringstream ss;
        ss << "Symbol \"" << node.identifier() << "\" already declared in this scope.";
        throw panic(ss.str(), node.location());
    }

    if (node.set_value()) {
        if (var_type_resolver.vla_dimensions().size() > 0) {
            std::ostringstream ss;
            ss << "Variable \"" << node.identifier() << "\" is a variable length array and cannot be initialized.";
            throw panic(ss.str(), node.location());
        }

        return logical_ir::variable_declaration(
            std::move(variable), 
            compile_expression(*node.set_value().value(), type)
        );
    }
    else if (var_type_resolver.vla_dimensions().size() > 0) {
        std::shared_ptr<typing::array_type> array_type = std::dynamic_pointer_cast<typing::array_type>(type.type());
        if (!array_type) {
            std::ostringstream ss;
            ss << "Variable \"" << node.identifier() << "\" is not an array.";
            throw panic(ss.str(), node.location());
        }
        
        return logical_ir::variable_declaration(
            std::move(variable), 
            std::make_unique<logical_ir::array_initializer>(std::move(var_type_resolver.release_vla_dimensions()), typing::qual_type(array_type->element_type()))
        );
    }
    else {
        std::ostringstream ss;
        ss << "Variable \"" << node.identifier() << "\" is not initialized.";
        throw panic(ss.str(), node.location());
    }
}

void compiler::top_level_compiler::visit(const ast::function_declaration& node) {
    std::shared_ptr<logical_ir::function_definition> function = std::dynamic_pointer_cast<logical_ir::function_definition>(
        m_compiler.m_symbol_explorer.lookup(node.function_name())
    );
    
    if (function->is_implemented()) {
        std::ostringstream ss;
        ss << "Function \"" << node.function_name() << "\" is already implemented.";
        throw panic(ss.str(), node.location());
    }

    m_compiler.m_symbol_explorer.visit(function);
    for (const auto& param : function->parameters()) {
        if (!m_compiler.m_symbol_explorer.add(param)) {
            std::ostringstream ss;
            ss << "Symbol \"" << param->name() << "\" already declared in this scope.";
            throw panic(ss.str(), node.location());
        }
    }

    std::vector<std::unique_ptr<logical_ir::statement>> statements;
    statements.reserve(node.function_body().statements().size());
    for (const auto& statement : node.function_body().statements()) {
        statements.emplace_back(m_compiler.m_statement_compiler(*statement));
    }
    function->implement(std::move(statements));
    m_compiler.m_symbol_explorer.exit();
}

void compiler::compile(const std::vector<std::unique_ptr<ast::ast_element>>& ast) {
    forward_declare_types forward_declare_types_pass(*this);
    for (const auto& element : ast) {
        element->accept(forward_declare_types_pass);
    }

    implement_type_declarations implement_type_declarations_pass(*this);
    for (const auto& element : ast) {
        element->accept(implement_type_declarations_pass);
    }

    forward_declare_functions forward_declare_functions_pass(*this);
    for (const auto& element : ast) {
        element->accept(forward_declare_functions_pass);
    }

    for (auto& [name, struct_type] : m_translation_unit.structs()) {
        check_layout_dependencies(struct_type);
    }
    for (auto& [name, union_type] : m_translation_unit.unions()) {
        check_layout_dependencies(union_type);
    }
    for (auto& [name, enum_type] : m_translation_unit.enums()) {
        check_layout_dependencies(enum_type);
    }

    m_symbol_explorer.visit(m_translation_unit.global_context());
    top_level_compiler top_level_compiler_pass(*this);
    for (const auto& element : ast) {
        element->accept(top_level_compiler_pass);
    }
    m_symbol_explorer.exit();

    for (const auto& symbol : m_translation_unit.global_symbols()) {
        std::shared_ptr<logical_ir::function_definition> function = std::dynamic_pointer_cast<logical_ir::function_definition>(symbol);
        if (function) {
            if (!function->is_implemented()) {
                std::ostringstream ss;
                ss << "Function \"" << function->name() << "\" is not implemented.";
                throw panic(ss.str(), function->location());
            }
        }
    }
}