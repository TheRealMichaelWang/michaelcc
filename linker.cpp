#include "linker.hpp"

using namespace michaelcc;

void linker::type_declare_pass::visit(const ast::struct_declaration& node) {
    if (node.feilds().empty()) return; // skip structs with no implementation

    if (m_linker.m_translation_unit.lookup_struct(node.struct_name().value()) != nullptr) {
        throw m_linker.panic("Redeclaration of struct " + node.struct_name().value(), node.location());
    }

    std::vector<typing::struct_type::field> fields;
    fields.reserve(node.feilds().size());
    for (const ast::variable_declaration& feild : node.feilds()) {
        fields.emplace_back(typing::struct_type::field{
            feild.identifier(),
            nullptr
        });
    }

    m_linker.m_translation_unit.declare_struct(std::make_unique<typing::struct_type>(node.struct_name().value(), std::move(fields)));
}

void linker::type_declare_pass::visit(const ast::union_declaration& node) {
    if (node.members().empty()) return; // skip unions with no implementation

    if (m_linker.m_translation_unit.lookup_union(node.union_name().value()) != nullptr) {
        throw m_linker.panic("Redeclaration of union " + node.union_name().value(), node.location());
    }

    std::vector<typing::union_type::member> members;
    members.reserve(node.members().size());
    for (const ast::union_declaration::member& member : node.members()) {
        members.emplace_back(typing::union_type::member{
            member.member_name,
            nullptr
        });
    }
    m_linker.m_translation_unit.declare_union(std::make_unique<typing::union_type>(node.union_name().value(), std::move(members)));
}

void linker::type_declare_pass::visit(const ast::enum_declaration& node) {
    if (node.enumerators().empty()) return; // skip enums with no implementation

    if (m_linker.m_translation_unit.lookup_enum(node.enum_name().value()) != nullptr) {
        throw m_linker.panic("Redeclaration of enum " + node.enum_name().value(), node.location());
    }

    std::vector<typing::enum_type::enumerator> enumerators;
    enumerators.reserve(node.enumerators().size());

    int64_t max_value = 0;
    for (const ast::enum_declaration::enumerator& enumerator : node.enumerators()) {
        enumerators.emplace_back(typing::enum_type::enumerator{
            enumerator.name,
            enumerator.value.value_or(max_value)
        });
        max_value = std::max(max_value, enumerator.value.value_or(max_value));
        max_value++;
    }
    m_linker.m_translation_unit.declare_enum(std::make_unique<typing::enum_type>(
        node.enum_name().value(), 
        std::move(enumerators), 
        std::make_optional<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS)
    ));
}

void linker::type_implement_pass::visit(const ast::struct_declaration& node) {
    auto struct_type = m_linker.m_translation_unit.lookup_struct(node.struct_name().value());
    if (struct_type == nullptr) {
        throw m_linker.panic("Undefined struct " + node.struct_name().value(), node.location());
    }
    if (struct_type->implemented()) {
        throw m_linker.panic("Struct " + node.struct_name().value() + " already implemented", node.location());
    }

    if (node.feilds().empty()) { return; } // skip structs with no implementation

    std::vector<std::unique_ptr<typing::type>> field_types;
    field_types.reserve(node.feilds().size());
    for (const ast::variable_declaration& field : node.feilds()) {
        field_types.emplace_back(m_linker.resolve_type(*field.type()));
    }

    if (!struct_type->implement_field_types(std::move(field_types))) {
        throw m_linker.panic("Invalid number of field types for struct " + node.struct_name().value(), node.location());
    }
}

void linker::type_implement_pass::visit(const ast::union_declaration& node) {
    auto union_type = m_linker.m_translation_unit.lookup_union(node.union_name().value());

    if (union_type == nullptr) {
        throw m_linker.panic("Undefined union " + node.union_name().value(), node.location());
    }
    if (union_type->implemented()) {
        throw m_linker.panic("Union " + node.union_name().value() + " already implemented", node.location());
    }

    if (node.members().empty()) { return; } // skip unions with no implementation

    std::vector<std::unique_ptr<typing::type>> member_types;
    member_types.reserve(node.members().size());
    for (const ast::union_declaration::member& member : node.members()) {
        member_types.emplace_back(m_linker.resolve_type(*member.member_type));
    }
    if (!union_type->implement_member_types(std::move(member_types))) {
        throw m_linker.panic("Invalid number of member types for union " + node.union_name().value(), node.location());
    }
}

void linker::function_declare_pass::visit(const ast::function_declaration& node) {
    
}