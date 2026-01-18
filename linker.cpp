#include "ast.hpp"
#include "semantic.hpp"
#include "logical.hpp"
#include <memory>

using namespace michaelcc;

void semantic_lowerer::forward_declare_types::visit(const ast::struct_declaration& node) {
    // Skip struct references (empty fields) or anonymous structs (no name)
    if (node.fields().empty() || !node.struct_name().has_value()) return;

    if (m_lowerer.m_translation_unit.lookup_struct(node.struct_name().value()) != nullptr) {
        throw m_lowerer.panic("Redeclaration of struct " + node.struct_name().value(), node.location());
    }

    std::vector<typing::member> fields;
    fields.reserve(node.fields().size());
    for (const ast::variable_declaration& field : node.fields()) {
        fields.emplace_back(typing::member{
            field.identifier(),
            typing::qual_type::owning(std::make_shared<typing::void_type>())
        });
    }

    m_lowerer.m_translation_unit.declare_struct(std::make_shared<typing::struct_type>(node.struct_name().value(), std::move(fields)));
}

void semantic_lowerer::forward_declare_types::visit(const ast::union_declaration& node) {
    // Skip union references (empty members) or anonymous unions (no name)
    if (node.members().empty() || !node.union_name().has_value()) return;

    if (m_lowerer.m_translation_unit.lookup_union(node.union_name().value()) != nullptr) {
        throw m_lowerer.panic("Redeclaration of union " + node.union_name().value(), node.location());
    }

    std::vector<typing::member> members;
    members.reserve(node.members().size());
    for (const ast::union_declaration::member& member : node.members()) {
        members.emplace_back(typing::member{
            member.member_name,
            typing::qual_type::owning(std::make_shared<typing::void_type>()),
            0
        });
    }
    m_lowerer.m_translation_unit.declare_union(std::make_shared<typing::union_type>(node.union_name().value(), std::move(members)));
}

void semantic_lowerer::forward_declare_types::visit(const ast::enum_declaration& node) {
    // Skip enum references (empty enumerators) or anonymous enums (no name)
    if (node.enumerators().empty() || !node.enum_name().has_value()) return;

    if (m_lowerer.m_translation_unit.lookup_enum(node.enum_name().value()) != nullptr) {
        throw m_lowerer.panic("Redeclaration of enum " + node.enum_name().value(), node.location());
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

    auto enum_type = std::make_shared<typing::enum_type>(
        node.enum_name().value(), 
        std::move(enumerators)
    );
    for (auto& enumerator : enum_type->enumerators()) {
        m_lowerer.m_translation_unit.declare_global(std::make_shared<logical_ir::enumerator_symbol>(
            typing::enum_type::enumerator(enumerator),
            std::shared_ptr<typing::enum_type>(enum_type),
            m_lowerer.m_translation_unit.global_context()
        ));
    }

    m_lowerer.m_translation_unit.declare_enum(std::move(enum_type));
}

void semantic_lowerer::implement_type_declarations::visit(const ast::struct_declaration& node) {
    // Skip anonymous structs or struct references without fields - they're handled inline during type resolution
    if (!node.struct_name().has_value() || node.fields().empty()) { return; }

    auto struct_type = m_lowerer.m_translation_unit.lookup_struct(node.struct_name().value());
    if (struct_type == nullptr) {
        throw m_lowerer.panic("Undefined struct " + node.struct_name().value(), node.location());
    }
    if (struct_type->is_implemented()) {
        throw m_lowerer.panic("Struct " + node.struct_name().value() + " already implemented", node.location());
    }

    std::vector<typing::qual_type> field_types;
    field_types.reserve(node.fields().size());
    for (const ast::variable_declaration& field : node.fields()) {
        field_types.emplace_back(m_lowerer.resolve_type(*field.type()));
    }

    if (!struct_type->implement_field_types(std::move(field_types))) {
        throw m_lowerer.panic("Invalid number of field types for struct " + node.struct_name().value(), node.location());
    }
}

void semantic_lowerer::implement_type_declarations::visit(const ast::union_declaration& node) {
    // Skip anonymous unions or union references without members - they're handled inline during type resolution
    if (!node.union_name().has_value() || node.members().empty()) { return; }

    auto union_type = m_lowerer.m_translation_unit.lookup_union(node.union_name().value());

    if (union_type == nullptr) {
        throw m_lowerer.panic("Undefined union " + node.union_name().value(), node.location());
    }
    if (union_type->is_implemented()) {
        throw m_lowerer.panic("Union " + node.union_name().value() + " already implemented", node.location());
    }

    std::vector<typing::qual_type> member_types;
    member_types.reserve(node.members().size());
    for (const ast::union_declaration::member& member : node.members()) {
        member_types.emplace_back(m_lowerer.resolve_type(*member.member_type));
    }
    if (!union_type->implement_member_types(std::move(member_types))) {
        throw m_lowerer.panic("Invalid number of member types for union " + node.union_name().value(), node.location());
    }
}

void semantic_lowerer::forward_declare_functions::forward_declare_function(const std::string& function_name, const ast::ast_element& return_type, const std::vector<ast::function_parameter>& parameters, const source_location& location) {
    std::vector<std::shared_ptr<logical_ir::variable>> logical_parameters;
    logical_parameters.reserve(parameters.size());
    for (const ast::function_parameter& parameter : parameters) {
        logical_parameters.emplace_back(std::make_shared<logical_ir::variable>(
            std::string(parameter.param_name),
            parameter.qualifiers,
            m_lowerer.resolve_type(*parameter.param_type).to_owning(),
            false,
            std::weak_ptr<logical_ir::symbol_context>()
        ));
    }

    auto function_definition = std::make_shared<logical_ir::function_definition>(
        std::string(function_name),
        m_lowerer.resolve_type(return_type).to_owning(),
        std::move(logical_parameters),
        std::weak_ptr<logical_ir::symbol_context>(m_lowerer.m_translation_unit.global_context()),
        source_location(location)
    );

    auto existing_symbol = m_lowerer.m_translation_unit.lookup_global(function_name);
    if (existing_symbol) {
        std::shared_ptr<logical_ir::function_definition> existing_function = std::dynamic_pointer_cast<logical_ir::function_definition>(existing_symbol);
        if (!existing_function) {
            throw m_lowerer.panic(std::format("Symbol {} is not a function", function_name), location);
        }

        if (existing_function->parameters().size() != parameters.size()) {
            throw m_lowerer.panic(std::format(
                "Parameter count mismatch for function {}; Function originally declared with {} parameters, but now declared with {} parameters.", 
                function_name,
                existing_function->parameters().size(),
                parameters.size()
            ), location);
        }

        for (size_t i = 0; i < parameters.size(); i++) {
            if (existing_function->parameters()[i]->get_type() != m_lowerer.resolve_type(*parameters[i].param_type)) {
                throw m_lowerer.panic(std::format(
                    "Parameter type mismatch for function {}; Function originally declared with parameter type {}, but now declared with parameter type {}.", 
                    function_name,
                    existing_function->parameters()[i]->get_type().to_string(),
                    logical_parameters[i]->get_type().to_string()
                ), location);
            }
        }

        if (existing_function->return_type() != m_lowerer.resolve_type(return_type)) {
            throw m_lowerer.panic(std::format(
                "Return type mismatch for function {}; Function originally declared with return type {}, but now declared with return type {}.", 
                function_name,
                existing_function->return_type().to_string(),
                m_lowerer.resolve_type(return_type).to_string()
            ), location);
        }
    } else {
        m_lowerer.m_translation_unit.declare_global(std::move(function_definition));
    }
}