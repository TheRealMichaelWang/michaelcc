#include "tokens.hpp"
#include "ast.hpp"
#include <cctype>    // For std::isprint
#include <iomanip>   // For std::hex, std::setw, std::setfill

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
        "STRINGIFY_IDENTIFIER",

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

const std::string michaelcc::token_to_str(const token tok)
{
    switch (tok.type())
    {
    case MICHAELCC_TOKEN_IDENTIFIER:
        return tok.string();
    case MICHAELCC_PREPROCESSOR_STRINGIFY_IDENTIFIER:
        return std::format("#{}", tok.string());
    case MICHAELCC_TOKEN_STRING_LITERAL:
        return std::format("\"{}\"", tok.string());
    default:
        return token_to_str(tok.type());
    }
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

void int_type::build_c_string(std::stringstream& out, int indent) const {
    print_int_qualifiers(out, m_qualifiers);
    switch (m_class) {
    case CHAR_INT_CLASS: out << "char"; break;
    case SHORT_INT_CLASS: out << "short"; break;
    case INT_INT_CLASS: out << "int"; break;
    case LONG_INT_CLASS: out << "long"; break;
    }
}

void float_type::build_c_string(std::stringstream& out, int indent) const {
    switch (m_class) {
    case FLOAT_FLOAT_CLASS: out << "float"; break;
    case DOUBLE_FLOAT_CLASS: out << "double"; break;
    }
}

void pointer_type::build_c_string(std::stringstream& out, int indent) const {
    m_pointee_type->build_c_string(out);
    out << "*";
}

void michaelcc::ast::pointer_type::build_declarator(std::stringstream& ss, const std::string& identifier) const {
    if (dynamic_cast<const array_type*>(m_pointee_type.get()) ||
        dynamic_cast<const function_pointer_type*>(m_pointee_type.get())) {
        // For array_type and function_pointer_type, call build_declarator
        if (auto* arr = dynamic_cast<const array_type*>(m_pointee_type.get())) {
            arr->build_declarator(ss, "(*" + identifier + ")");
        } else if (auto* fptr = dynamic_cast<const function_pointer_type*>(m_pointee_type.get())) {
            fptr->build_declarator(ss, "(*" + identifier + ")");
        }
    }
    else {
        build_c_string(ss);
        ss << ' ' << identifier;
    }
}

void array_type::build_c_string(std::stringstream& out, int indent) const {
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
    // For array_type element, call build_declarator if available
    if (auto* arr = dynamic_cast<const array_type*>(m_element_type.get())) {
        arr->build_declarator(ss, identifier + "[" + length_str + "]");
    } else if (auto* ptr = dynamic_cast<const pointer_type*>(m_element_type.get())) {
        ptr->build_declarator(ss, identifier + "[" + length_str + "]");
    } else if (auto* fptr = dynamic_cast<const function_pointer_type*>(m_element_type.get())) {
        fptr->build_declarator(ss, identifier + "[" + length_str + "]");
    } else {
        m_element_type->build_c_string(ss);
        ss << ' ' << identifier << "[" << length_str << "]";
    }
}

void ast::function_pointer_type::build_c_string(std::stringstream& ss, int indent) const {
    m_return_type->build_c_string(ss);
    ss << "(*)(";
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
    // For return type, call build_declarator if available
    if (auto* arr = dynamic_cast<const array_type*>(m_return_type.get())) {
        arr->build_declarator(ss, "(*" + identifier + ")(" + params_str + ")");
    } else if (auto* ptr = dynamic_cast<const pointer_type*>(m_return_type.get())) {
        ptr->build_declarator(ss, "(*" + identifier + ")(" + params_str + ")");
    } else if (auto* fptr = dynamic_cast<const function_pointer_type*>(m_return_type.get())) {
        fptr->build_declarator(ss, "(*" + identifier + ")(" + params_str + ")");
    } else {
        m_return_type->build_c_string(ss);
        ss << " (*" << identifier << ")(" << params_str << ")";
    }
}

void context_block::build_c_string(std::stringstream& ss, int indent) const {
    ss << "{\n";
    for (const auto& stmt : m_statements) {
        stmt->build_c_string(ss, indent + 1);
        ss << '\n';
    }
    ss << indent_str(indent) << "}";
}

void for_loop::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "for (";
    if (m_initial_statement) m_initial_statement->build_c_string(ss, 0);
    ss << "; ";
    if (m_condition) m_condition->build_c_string(ss, 0);
    ss << "; ";
    if (m_increment_statement) m_increment_statement->build_c_string(ss, 0);
    ss << ") ";
    m_to_execute.build_c_string(ss, indent);
}

void do_block::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "do ";
    m_to_execute.build_c_string(ss, indent);
    ss << " while (";
    if (m_condition) m_condition->build_c_string(ss, 0);
    ss << ");";
}

void while_block::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "while (";
    if (m_condition) m_condition->build_c_string(ss, 0);
    ss << ") ";
    m_to_execute.build_c_string(ss, indent);
}

void if_block::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "if (";
    if (m_condition) m_condition->build_c_string(ss, 0);
    ss << ") ";
    m_execute_if_true.build_c_string(ss, indent);
}

void if_else_block::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "if (";
    if (m_condition) m_condition->build_c_string(ss, 0);
    ss << ") ";
    m_execute_if_true.build_c_string(ss, indent);
    ss << " else ";
    m_execute_if_false.build_c_string(ss, indent);
}

void return_statement::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "return";
    if (value) {
        ss << " ";
        value->build_c_string(ss, 0);
    }
    ss << ";";
}

void break_statement::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "break";
    if (loop_depth > 1) {
        ss << " " << loop_depth;
    }
    ss << ";";
}

void continue_statement::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent) << "continue";
    if (loop_depth > 1) {
        ss << " " << loop_depth;
    }
    ss << ";";
}

void int_literal::build_c_string(std::stringstream& ss, int indent) const {
    ss << m_value;
}

void float_literal::build_c_string(std::stringstream& ss, int indent) const {
    ss << m_value << "f";
}

void double_literal::build_c_string(std::stringstream& ss, int indent) const {
    ss << m_value;
}

void escape_char(char c, std::ostream& out) {
    switch (c) {
    case '\\': out << "\\\\"; break;  // Backslash
    case '"':  out << "\\\""; break;  // Double quote
    case '\n': out << "\\n";  break;  // Newline
    case '\r': out << "\\r";  break;  // Carriage return
    case '\t': out << "\\t";  break;  // Tab
    case '\a': out << "\\a";  break;  // Bell
    case '\b': out << "\\b";  break;  // Backspace
    case '\f': out << "\\f";  break;  // Form feed
    case '\v': out << "\\v";  break;  // Vertical tab
    default:
        if (std::isprint(static_cast<unsigned char>(c))) {
            out << c;  // Printable characters as-is
        }
        else {
            out << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(static_cast<unsigned char>(c));
        }
        break;
    }
}

void string_literal::build_c_string(std::stringstream & ss, int indent) const {
    ss << "\"";
    for (char c : m_value) {
        escape_char(c, ss);
    }
    ss << "\"";
}

void variable_reference::build_c_string(std::stringstream& ss, int indent) const {
    ss << m_identifier;
}

void get_index::build_c_string(std::stringstream& ss, int indent) const {
    m_ptr->build_c_string(ss, 0);
    ss << "[";
    m_index->build_c_string(ss, 0);
    ss << "]";
}

void get_property::build_c_string(std::stringstream& ss, int indent) const {
    m_struct->build_c_string(ss, 0);
    ss << (m_is_pointer_dereference ? "->" : ".");
    ss << m_property_name;
}

void set_operator::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent);
    m_set_dest->build_c_string(ss, 0);
    ss << " = ";
    m_set_value->build_c_string(ss, 0);
    ss << ";";
}

void dereference_operator::build_c_string(std::stringstream& ss, int indent) const {
    ss << "*";
    m_pointer->build_c_string(ss, 0);
}

void get_reference::build_c_string(std::stringstream& ss, int indent) const {
    ss << "&";
    m_item->build_c_string(ss, 0);
}

void arithmetic_operator::build_c_string(std::stringstream& ss, int indent) const {
    ss << "(";
    m_left->build_c_string(ss, 0);
    ss << " " << token_to_str(m_operation) << " ";
    m_right->build_c_string(ss, 0);
    ss << ")";
}

void conditional_expression::build_c_string(std::stringstream& ss, int indent) const {
    ss << "(";
    m_condition->build_c_string(ss, 0);
    ss << " ? ";
    m_true_expr->build_c_string(ss, 0);
    ss << " : ";
    m_false_expr->build_c_string(ss, 0);
    ss << ")";
}

void function_call::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent);
    m_callee->build_c_string(ss, 0);
    ss << "(";
    for (size_t i = 0; i < m_arguments.size(); ++i) {
        if (i > 0) ss << ", ";
        m_arguments[i]->build_c_string(ss, 0);
    }
    ss << ");";
}

void initializer_list_expression::build_c_string(std::stringstream& ss, int indent) const {
    ss << "{";
    for (size_t i = 0; i < m_initializers.size(); ++i) {
        if (i > 0) ss << ", ";
        m_initializers[i]->build_c_string(ss, 0);
    }
    ss << "}";
}

// Helper function to call build_declarator on type elements
static void call_build_declarator(const ast_element* type, std::stringstream& ss, const std::string& identifier) {
    if (auto* int_t = dynamic_cast<const int_type*>(type)) {
        int_t->build_declarator(ss, identifier);
    } else if (auto* float_t = dynamic_cast<const float_type*>(type)) {
        float_t->build_declarator(ss, identifier);
    } else if (auto* void_t = dynamic_cast<const void_type*>(type)) {
        void_t->build_declarator(ss, identifier);
    } else if (auto* ptr = dynamic_cast<const pointer_type*>(type)) {
        ptr->build_declarator(ss, identifier);
    } else if (auto* arr = dynamic_cast<const array_type*>(type)) {
        arr->build_declarator(ss, identifier);
    } else if (auto* fptr = dynamic_cast<const function_pointer_type*>(type)) {
        fptr->build_declarator(ss, identifier);
    } else if (auto* strct = dynamic_cast<const struct_declaration*>(type)) {
        strct->build_declarator(ss, identifier);
    } else if (auto* enm = dynamic_cast<const enum_declaration*>(type)) {
        enm->build_declarator(ss, identifier);
    } else if (auto* un = dynamic_cast<const union_declaration*>(type)) {
        un->build_declarator(ss, identifier);
    } else {
        type->build_c_string(ss);
        ss << ' ' << identifier;
    }
}

void variable_declaration::build_c_string(std::stringstream& ss, int indent) const {
    ss << indent_str(indent);
    if (m_qualifiers & CONST_STORAGE_QUALIFIER) ss << "const ";
    if (m_qualifiers & VOLATILE_STORAGE_QUALIFIER) ss << "volatile ";
    if (m_qualifiers & EXTERN_STORAGE_QUALIFIER) ss << "extern ";
    if (m_qualifiers & STATIC_STORAGE_QUALIFIER) ss << "static ";
    if (m_qualifiers & REGISTER_STORAGE_QUALIFIER) ss << "register ";
    call_build_declarator(m_type.get(), ss, m_identifier);
    if (m_set_value.has_value()) {
        ss << " = ";
        m_set_value.value()->build_c_string(ss, 0);
    }
    ss << ";";
}

void typedef_declaration::build_c_string(std::stringstream& out, int indent) const {
    out << "typedef ";
    call_build_declarator(m_type.get(), out, m_name);
    out << ";";
}

void struct_declaration::build_c_string(std::stringstream& out, int indent) const {
    out << "struct";
    if (m_struct_name) {
        out << " " << m_struct_name.value();
    }

    if (m_members.empty()) {
        return;
    }
    out << " {\n";
    for (const auto& member : m_members) {
        member.build_c_string(out, indent + 1);
        out << "\n";
    }
    out << "}";
}

void enum_declaration::build_c_string(std::stringstream& out, int indent) const {
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

void union_declaration::build_c_string(std::stringstream& out, int indent) const {
    out << "union";
    if (m_union_name) out << " " << m_union_name.value();
    if (m_members.empty()) {
        return;
    }
    out << " {\n";
    for (const auto& member : m_members) {
        out << "  ";
        member.member_type->build_c_string(out, 0);
        out << " " << member.member_name << ";\n";
    }
    out << "}";
}

void function_prototype::build_c_string(std::stringstream& out, int indent) const {
    m_return_type->build_c_string(out, 0);
    out << " " << m_function_name << "(";
    for (size_t i = 0; i < m_parameters.size(); ++i) {
        m_parameters[i].param_type->build_c_string(out, 0);
        out << " " << m_parameters[i].param_name;
        if (i + 1 < m_parameters.size()) out << ", ";
    }
    out << ");";
}

void function_declaration::build_c_string(std::stringstream& out, int indent) const {
    m_return_type->build_c_string(out, 0);
    out << " " << m_function_name << "(";
    for (size_t i = 0; i < m_parameters.size(); ++i) {
        m_parameters[i].param_type->build_c_string(out, 0);
        out << " " << m_parameters[i].param_name;
        if (i + 1 < m_parameters.size()) out << ", ";
    }
    out << ')';
    m_function_body.build_c_string(out, indent);
}
