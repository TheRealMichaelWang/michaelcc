#include "tokens.hpp"
#include "ast.hpp"

using namespace michaelcc;
using namespace michaelcc::ast;

const std::string michaelcc::token_to_str(token_type type) {
	static const char* tok_names[] = {
		//preprocessor tokens
		"#define",
		"#ifdef",
		"#ifndef",
		"#else",
		"#endif",
		"#include",
		"#line",

		//punctuators
		"[",
		"]",
		"(",
		")",
		"{",
		"}",
		",",
		":",
		";",
		"=",
		".",
		"~",
		"->",

		//keywords
		"auto",
		"break",
		"case",
		"char",
		"const",
		"continue",
		"default",
		"do",
		"double",
		"else",
		"enum",
		"extern",
		"float",
		"for",
		"goto",
		"if",
		"int",
		"long",
		"register",
		"return",
		"short",
		"signed",
		"sizeof",
		"static",
		"struct",
		"switch",
		"typedef",
		"union",
		"unsigned",
		"void",
		"volatile",
		"while",
		"IDENTIFIER",

		//operators
		"+",
		"-",
        "*",
		"++",
		"--",
		"+=",
		"-=",
		"/",
		"^",
		"&",
		"|",
		"&&",
		"||",
		">",
		"<",
		">=",
		"<=",
		"==",
		"?",

		//literal tokens
		"INTEGER_LITERAL",
		"CHAR_LITERAL",
		"FLOAT32_LITERAL",
		"FLOAT64_LITERAL",
		"STRING_LITERAL",

		//control tokens
		"\\n",
		"END"
	};
	return tok_names[type];
}

// Helper for indentation
static std::string indent_str(int indent) {
    return std::string(indent * 2, ' ');
}

// Helper for int type qualifiers
static void print_int_qualifiers(std::stringstream& out, uint8_t q) {
    if (q & UNSIGNED_INT_QUALIFIER) out << "unsigned ";
    if (q & SIGNED_INT_QUALIFIER) out << "signed ";
    if (q & LONG_INT_QUALIFIER) out << "long ";
}

void int_type::build_c_string(std::stringstream& out) const {
    print_int_qualifiers(out, m_qualifiers);
    switch (m_class) {
    case CHAR_INT_CLASS: out << "char"; break;
    case SHORT_INT_CLASS: out << "short"; break;
    case INT_INT_CLASS: out << "int"; break;
    case LONG_INT_CLASS: out << "long"; break;
    }
}

void float_type::build_c_string(std::stringstream& out) const {
    switch (m_class) {
    case FLOAT_FLOAT_CLASS: out << "float"; break;
    case DOUBLE_FLOAT_CLASS: out << "double"; break;
    }
}

void pointer_type::build_c_string(std::stringstream& out) const {
    m_pointee_type->build_c_string(out);
    out << "*";
}

void michaelcc::ast::pointer_type::build_declarator(std::stringstream& ss, const std::string& identifier) const {
    if (dynamic_cast<const array_type*>(m_pointee_type.get()) ||
        dynamic_cast<const function_pointer_type*>(m_pointee_type.get())) {
        m_pointee_type->build_declarator(ss, "(*" + identifier + ")");
    }
    else {
        ast::type::build_declarator(ss, identifier);
    }
}

void array_type::build_c_string(std::stringstream& out) const {
    m_element_type->build_c_string(out);
    out << "[";
    if (m_length && m_length.value()) {
        m_length.value()->build_c_string(out);
    }
    out << "]";
}

void michaelcc::ast::array_type::build_declarator(std::stringstream& ss, const std::string& identifier) const {
    std::string length_str;
    if (m_length.has_value()) {
        std::stringstream length_ss;
        m_length.value()->build_c_string(length_ss);
        length_str = length_ss.str();
    }
    m_element_type->build_declarator(ss, identifier + "[" + length_str + "]");
}

void ast::function_pointer_type::build_c_string(std::stringstream& ss) const {
    m_return_type->build_c_string(ss);
    ss << " (*)(";
    for (size_t i = 0; i < m_parameter_types.size(); ++i) {
        if (i > 0) ss << ", ";
        m_parameter_types[i]->build_c_string(ss);
    }
    ss << ")";
}

void michaelcc::ast::function_pointer_type::build_declarator(std::stringstream& ss, const std::string& identifier) const {
    std::string params_str;
    std::stringstream params_ss;
    for (size_t i = 0; i < m_parameter_types.size(); ++i) {
        if (i > 0) params_ss << ", ";
        m_parameter_types[i]->build_c_string(params_ss);
    }
    params_str = params_ss.str();
    m_return_type->build_declarator(ss, "(*" + identifier + ")(" + params_str + ")");
}

void context_block::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << "{\n";
    for (const auto& stmt : m_statements) {
        stmt->build_c_string_indent(ss, indent + 1);
        ss << '\n';
    }
    ss << indent_str(indent) << "}";
}

void for_loop::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "for (";
    if (m_initial_statement) m_initial_statement->build_c_string(ss);
    ss << "; ";
    if (m_condition) m_condition->build_c_string(ss);
    ss << "; ";
    if (m_increment_statement) m_increment_statement->build_c_string(ss);
    ss << ") ";
    m_to_execute.build_c_string_indent(ss, indent);
}

void do_block::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "do ";
    m_to_execute.build_c_string_indent(ss, indent);
    ss << " while (";
    if (m_condition) m_condition->build_c_string(ss);
    ss << ");";
}

void while_block::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "while (";
    if (m_condition) m_condition->build_c_string(ss);
    ss << ") ";
    m_to_execute.build_c_string_indent(ss, indent);
}

void if_block::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "if (";
    if (m_condition) m_condition->build_c_string(ss);
    ss << ") ";
    m_execute_if_true.build_c_string_indent(ss, indent);
}

void if_else_block::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "if (";
    if (m_condition) m_condition->build_c_string(ss);
    ss << ") ";
    m_execute_if_true.build_c_string_indent(ss, indent);
    ss << " else ";
    m_execute_if_false.build_c_string_indent(ss, indent);
}

void return_statement::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "return";
    if (value) {
        ss << " ";
        value->build_c_string(ss);
    }
    ss << ";";
}

void break_statement::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "break";
    if (loop_depth > 1) {
        ss << " " << loop_depth;
    }
    ss << ";";
}

void continue_statement::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "continue";
    if (loop_depth > 1) {
        ss << " " << loop_depth;
    }
    ss << ";";
}

void int_literal::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    ss << m_value;
}

void float_literal::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    ss << m_value << "f";
}

void double_literal::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    ss << m_value;
}

void variable_reference::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    ss << m_identifier;
}

void get_index::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    int current_precedence = 15; // Postfix operator precedence
    if (current_precedence < parent_precedence) ss << "(";
    m_ptr->build_c_string_prec(ss, current_precedence);
    ss << "[";
    m_index->build_c_string_prec(ss, 0);
    ss << "]";
    if (current_precedence < parent_precedence) ss << ")";
}

void get_property::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    int current_precedence = 15; // Postfix operator precedence
    if (current_precedence < parent_precedence) ss << "(";
    m_struct->build_c_string_prec(ss, current_precedence);
    ss << (m_is_pointer_dereference ? "->" : ".");
    ss << m_property_name;
    if (current_precedence < parent_precedence) ss << ")";
}

void set_operator::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent);
    m_set_dest->build_c_string(ss);
    ss << " = ";
    m_set_value->build_c_string(ss);
    ss << ";";
}

void set_operator::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    int current_precedence = 1; // Assignment operator precedence
    if (current_precedence < parent_precedence) ss << "(";
    m_set_dest->build_c_string_prec(ss, current_precedence);
    ss << " = ";
    m_set_value->build_c_string_prec(ss, current_precedence);
    if (current_precedence < parent_precedence) ss << ")";
}

void dereference_operator::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    int current_precedence = 14; // Unary operator precedence
    if (current_precedence < parent_precedence) ss << "(";
    ss << "*";
    m_pointer->build_c_string_prec(ss, current_precedence);
    if (current_precedence < parent_precedence) ss << ")";
}

void get_reference::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    int current_precedence = 14; // Unary operator precedence
    if (current_precedence < parent_precedence) ss << "(";
    ss << "&";
    m_item->build_c_string_prec(ss, current_precedence);
    if (current_precedence < parent_precedence) ss << ")";
}

void arithmetic_operator::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    int current_precedence = operator_precedence.at(m_operation);
    if (current_precedence < parent_precedence) ss << "(";
    m_left->build_c_string_prec(ss, current_precedence);
    ss << " " << token_to_str(m_operation) << " ";
    m_right->build_c_string_prec(ss, current_precedence + 1); // +1 for left-associative operators
    if (current_precedence < parent_precedence) ss << ")";
}

void conditional_expression::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    int current_precedence = 2; // Ternary operator precedence
    if (current_precedence < parent_precedence) ss << "(";
    m_condition->build_c_string_prec(ss, current_precedence + 1);
    ss << " ? ";
    m_true_expr->build_c_string_prec(ss, current_precedence + 1);
    ss << " : ";
    m_false_expr->build_c_string_prec(ss, current_precedence); // Right-associative
    if (current_precedence < parent_precedence) ss << ")";
}

void function_call::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent);
    m_callee->build_c_string(ss);
    ss << "(";
    for (size_t i = 0; i < m_arguments.size(); ++i) {
        if (i > 0) ss << ", ";
        m_arguments[i]->build_c_string(ss);
    }
    ss << ");";
}

void function_call::build_c_string_prec(std::stringstream& ss, int parent_precedence) const {
    int current_precedence = 15; // Postfix operator precedence
    if (current_precedence < parent_precedence) ss << "(";
    m_callee->build_c_string_prec(ss, current_precedence);
    ss << "(";
    for (size_t i = 0; i < m_arguments.size(); ++i) {
        if (i > 0) ss << ", ";
        m_arguments[i]->build_c_string(ss);
    }
    ss << ")";
    if (current_precedence < parent_precedence) ss << ")";
}

void initializer_list_expression::build_c_string_prec(std::stringstream& ss, int indent) const {
    ss << "{";
    for (size_t i = 0; i < m_initializers.size(); ++i) {
        if (i > 0) ss << ", ";
        m_initializers[i]->build_c_string_prec(ss, 0);
    }
    ss << "}";
}

void variable_declaration::build_c_string_indent(std::stringstream& ss, int indent) const {
    ss << indent_str(indent);
    if (m_qualifiers & CONST_STORAGE_QUALIFIER) ss << "const ";
    if (m_qualifiers & VOLATILE_STORAGE_QUALIFIER) ss << "volatile ";
    if (m_qualifiers & EXTERN_STORAGE_QUALIFIER) ss << "extern ";
    if (m_qualifiers & STATIC_STORAGE_QUALIFIER) ss << "static ";
    if (m_qualifiers & REGISTER_STORAGE_QUALIFIER) ss << "register ";
    m_type->build_declarator(ss, m_identifier);
    if (m_set_value.has_value()) {
        ss << " = ";
        m_set_value.value()->build_c_string(ss);
    }
    ss << ";";
}

void typedef_declaration::build_c_string(std::stringstream& out) const {
    out << "typedef ";
    m_type->build_declarator(out, m_name);
    out << ";";
}

void struct_declaration::build_c_string(std::stringstream& out) const {
    out << "struct";
    if (m_struct_name) {
        out << " " << m_struct_name.value();
    }

    if (m_members.empty()) {
        return;
    }
    out << " {\n";
    for (const auto& member : m_members) {
        member.build_c_string(out);
        out << "\n";
    }
    out << "}";
}

void enum_declaration::build_c_string(std::stringstream& out) const {
    out << "enum";
    if (m_enum_name) out << " " << m_enum_name.value();
    if (m_enumerators.empty()) {
        return;
    }
    out << " {\n";
    for (size_t i = 0; i < m_enumerators.size(); ++i) {
        out << "  " << m_enumerators[i].name;
        if (m_enumerators[i].value) {
            out << " = " << m_enumerators[i].value.value();
        }
        if (i + 1 < m_enumerators.size()) out << ",";
        out << "\n";
    }
    out << "}";
}

void union_declaration::build_c_string(std::stringstream& out) const {
    out << "union";
    if (m_union_name) out << " " << m_union_name.value();
    if (m_members.empty()) {
        return;
    }
    out << " {\n";
    for (const auto& member : m_members) {
        out << "  ";
        member.member_type->build_c_string(out);
        out << " " << member.member_name << ";\n";
    }
    out << "}";
}

void function_prototype::build_c_string(std::stringstream& out) const {
    m_return_type->build_c_string(out);
    out << " " << m_function_name << "(";
    for (size_t i = 0; i < m_parameters.size(); ++i) {
        m_parameters[i].param_type->build_c_string(out);
        out << " " << m_parameters[i].param_name;
        if (i + 1 < m_parameters.size()) out << ", ";
    }
    out << ");";
}

void function_declaration::build_c_string(std::stringstream& out) const {
    m_return_type->build_c_string(out);
    out << " " << m_function_name << "(";
    for (size_t i = 0; i < m_parameters.size(); ++i) {
        m_parameters[i].param_type->build_c_string(out);
        out << " " << m_parameters[i].param_name;
        if (i + 1 < m_parameters.size()) out << ", ";
    }
    out << ')';
    m_function_body.build_c_string(out);
}