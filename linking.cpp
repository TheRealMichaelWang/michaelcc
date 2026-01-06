#include "linker.hpp"

using namespace michaelcc;

void linker::type_declare_pass::visit(const ast::struct_declaration& node) {
    if (node.feilds().empty()) return; // skip structs with no implementation

    auto type_context = &m_linker.m_translation_unit.types();

    if (type_context->lookup_struct(node.struct_name().value()) != nullptr) {
        throw m_linker.panic("Redeclaration of struct " + node.struct_name().value(), node.location());
    }

    std::vector<logical_ir::struct_type::field> fields;
    fields.reserve(node.feilds().size());
    for (const ast::variable_declaration& feild : node.feilds()) {
        fields.emplace_back(logical_ir::struct_type::field{
            feild.identifier(),
            nullptr,
            0
        });
    }

    type_context->declare_struct(std::string(node.struct_name().value()), std::move(fields), 0, 0);
}

void linker::type_declare_pass::visit(const ast::union_declaration& node) {
    if (node.members().empty()) return; // skip unions with no implementation

    auto type_context = &m_linker.m_translation_unit.types();

    if (type_context->lookup_union(std::string(node.union_name().value())) != nullptr) {
        throw m_linker.panic("Redeclaration of union " + node.union_name().value(), node.location());
    }

    std::vector<logical_ir::union_type::member> members;
    members.reserve(node.members().size());
    for (const ast::union_declaration::member& member : node.members()) {
        members.emplace_back(logical_ir::union_type::member{
            member.member_name,
            nullptr
        });
    }
    type_context->declare_union(std::string(node.union_name().value()), std::move(members), 0, 0);
}

void linker::type_declare_pass::visit(const ast::enum_declaration& node) {
    if (node.enumerators().empty()) return; // skip enums with no implementation

    auto type_context = &m_linker.m_translation_unit.types();

    if (type_context->lookup_enum(std::string(node.enum_name().value())) != nullptr) {
        throw m_linker.panic("Redeclaration of enum " + node.enum_name().value(), node.location());
    }

    std::vector<logical_ir::enum_type::enumerator> enumerators;
    enumerators.reserve(node.enumerators().size());

    int64_t max_value = 0;
    for (const ast::enum_declaration::enumerator& enumerator : node.enumerators()) {
        enumerators.emplace_back(logical_ir::enum_type::enumerator{
            enumerator.name,
            enumerator.value.value_or(max_value)
        });
        max_value = std::max(max_value, enumerator.value.value_or(max_value));
        max_value++;
    }
    type_context->declare_enum(std::string(node.enum_name().value()), std::move(enumerators), nullptr);
}

void linker::type_link_pass::visit(const ast::struct_declaration& node) {
    auto type_context = &m_linker.m_translation_unit.types();
    auto struct_type = type_context->lookup_struct(std::string(node.struct_name().value()));

    if (struct_type == nullptr) {
        throw m_linker.panic("Undefined struct " + node.struct_name().value(), node.location());
    }
    
    if (node.feilds().empty()) { return; } // skip structs with no implementation

    std::vector<logical_ir::type*> field_types;
    field_types.reserve(node.feilds().size());
    for (const ast::variable_declaration& field : node.feilds()) {
        field_types.push_back(m_linker.resolve_type(*field.type()));
    }
    type_context->implement_struct_field_types(std::string(node.struct_name().value()), std::move(field_types));
}

void linker::type_link_pass::visit(const ast::union_declaration& node) {
    auto type_context = &m_linker.m_translation_unit.types();
    auto union_type = type_context->lookup_union(std::string(node.union_name().value()));

    if (union_type == nullptr) {
        throw m_linker.panic("Undefined union " + node.union_name().value(), node.location());
    }

    if (node.members().empty()) { return; } // skip unions with no implementation

    std::vector<logical_ir::type*> member_types;
    member_types.reserve(node.members().size());
    for (const ast::union_declaration::member& member : node.members()) {
        member_types.push_back(m_linker.resolve_type(*member.member_type));
    }
    type_context->implement_union_member_types(std::string(node.union_name().value()), std::move(member_types));
}