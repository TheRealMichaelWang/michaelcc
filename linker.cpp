#include "compiler.hpp"
#include "logical.hpp"
#include <memory>

using namespace michaelcc;

void compiler::forward_declare_types::visit(const ast::struct_declaration& node) {
    if (node.feilds().empty()) return; // skip structs with no implementation

    if (m_compiler.m_translation_unit.lookup_struct(node.struct_name().value()) != nullptr) {
        throw m_compiler.panic("Redeclaration of struct " + node.struct_name().value(), node.location());
    }

    std::vector<typing::struct_type::field> fields;
    fields.reserve(node.feilds().size());
    for (const ast::variable_declaration& feild : node.feilds()) {
        fields.emplace_back(typing::struct_type::field{
            feild.identifier(),
            nullptr
        });
    }

    m_compiler.m_translation_unit.declare_struct(std::make_unique<typing::struct_type>(node.struct_name().value(), std::move(fields)));
}

void compiler::forward_declare_types::visit(const ast::union_declaration& node) {
    if (node.members().empty()) return; // skip unions with no implementation

    if (m_compiler.m_translation_unit.lookup_union(node.union_name().value()) != nullptr) {
        throw m_compiler.panic("Redeclaration of union " + node.union_name().value(), node.location());
    }

    std::vector<typing::union_type::member> members;
    members.reserve(node.members().size());
    for (const ast::union_declaration::member& member : node.members()) {
        members.emplace_back(typing::union_type::member{
            member.member_name,
            nullptr
        });
    }
    m_compiler.m_translation_unit.declare_union(std::make_unique<typing::union_type>(node.union_name().value(), std::move(members)));
}

void compiler::forward_declare_types::visit(const ast::enum_declaration& node) {
    if (node.enumerators().empty()) return; // skip enums with no implementation

    if (m_compiler.m_translation_unit.lookup_enum(node.enum_name().value()) != nullptr) {
        throw m_compiler.panic("Redeclaration of enum " + node.enum_name().value(), node.location());
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
    m_compiler.m_translation_unit.declare_enum(std::make_unique<typing::enum_type>(
        node.enum_name().value(), 
        std::move(enumerators), 
        std::make_optional<typing::int_type>(typing::NO_INT_QUALIFIER, typing::INT_INT_CLASS)
    ));
}

void compiler::implement_type_declarations::visit(const ast::struct_declaration& node) {
    auto struct_type = m_compiler.m_translation_unit.lookup_struct(node.struct_name().value());
    if (struct_type == nullptr) {
        throw m_compiler.panic("Undefined struct " + node.struct_name().value(), node.location());
    }
    if (struct_type->implemented()) {
        throw m_compiler.panic("Struct " + node.struct_name().value() + " already implemented", node.location());
    }

    if (node.feilds().empty()) { return; } // skip structs with no implementation

    std::vector<std::unique_ptr<typing::type>> field_types;
    field_types.reserve(node.feilds().size());
    for (const ast::variable_declaration& field : node.feilds()) {
        field_types.emplace_back(m_compiler.resolve_type(*field.type()));
    }

    if (!struct_type->implement_field_types(std::move(field_types))) {
        throw m_compiler.panic("Invalid number of field types for struct " + node.struct_name().value(), node.location());
    }
}

void compiler::implement_type_declarations::visit(const ast::union_declaration& node) {
    auto union_type = m_compiler.m_translation_unit.lookup_union(node.union_name().value());

    if (union_type == nullptr) {
        throw m_compiler.panic("Undefined union " + node.union_name().value(), node.location());
    }
    if (union_type->implemented()) {
        throw m_compiler.panic("Union " + node.union_name().value() + " already implemented", node.location());
    }

    if (node.members().empty()) { return; } // skip unions with no implementation

    std::vector<std::unique_ptr<typing::type>> member_types;
    member_types.reserve(node.members().size());
    for (const ast::union_declaration::member& member : node.members()) {
        member_types.emplace_back(m_compiler.resolve_type(*member.member_type));
    }
    if (!union_type->implement_member_types(std::move(member_types))) {
        throw m_compiler.panic("Invalid number of member types for union " + node.union_name().value(), node.location());
    }
}

void compiler::forward_declare_functions::forward_declare_function(const std::string& function_name, const std::vector<ast::function_parameter>& parameters, const source_location& location) {
    std::vector<std::shared_ptr<logical_ir::variable>> logical_parameters;
    logical_parameters.reserve(parameters.size());
    for (const ast::function_parameter& parameter : parameters) {
        logical_parameters.emplace_back(std::make_shared<logical_ir::variable>(
            std::string(parameter.param_name),
            m_compiler.resolve_type(*parameter.param_type),
            false,
            std::weak_ptr<logical_ir::symbol_context>()
        ));
    }

    auto function_definition = std::make_shared<logical_ir::function_definition>(
        std::string(function_name),
        std::move(logical_parameters),
        std::weak_ptr<logical_ir::symbol_context>(m_compiler.m_translation_unit.global_context())
    );

    auto existing_function = m_compiler.m_translation_unit.lookup_global(function_name);
    if (existing_function) {
        logical_ir::function_definition* existing_function_definition = dynamic_cast<logical_ir::function_definition*>(existing_function.get());
        if (existing_function_definition == nullptr) {
            throw m_compiler.panic(std::format("Symbol {} is not a function", function_name), location);
        }

        if (existing_function_definition->parameters().size() != parameters.size()) {
            throw m_compiler.panic(std::format(
                "Parameter count mismatch for function {}; Function originally declared with {} parameters, but now declared with {} parameters.", 
                function_name,
                existing_function_definition->parameters().size(),
                parameters.size()
            ), location);
        }

        for (size_t i = 0; i < parameters.size(); i++) {
            if (!typing::are_quivalent(*existing_function_definition->parameters()[i]->get_type(), *m_compiler.resolve_type(*parameters[i].param_type))) {
                throw m_compiler.panic(std::format(
                    "Parameter type mismatch for function {}; Function originally declared with parameter type {}, but now declared with parameter type {}.", 
                    function_name,
                    existing_function_definition->parameters()[i]->get_type()->to_string(),
                    logical_parameters[i]->get_type()->to_string()
                ), location);
            }
        }
    } else {
        m_compiler.m_translation_unit.declare_global(std::move(function_definition));
    }
}