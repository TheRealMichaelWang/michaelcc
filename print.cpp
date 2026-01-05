#include "tokens.hpp"
#include "ast.hpp"
#include <cctype>
#include <iomanip>

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

class print_visitor : public visitor {
private:
    std::stringstream& m_out;
    int m_indent;
    bool m_print_requested = false;
    int m_parent_precedence = 0;

    std::string indent_str() const {
        return std::string(m_indent * 2, ' ');
    }

    void print_int_qualifiers(uint8_t q) {
        if (q & UNSIGNED_INT_QUALIFIER) m_out << "unsigned ";
        if (q & SIGNED_INT_QUALIFIER) m_out << "signed ";
        if (q & LONG_INT_QUALIFIER) m_out << "long ";
    }

    void escape_char(char c) {
        switch (c) {
        case '\\': m_out << "\\\\"; break;
        case '"':  m_out << "\\\""; break;
        case '\n': m_out << "\\n";  break;
        case '\r': m_out << "\\r";  break;
        case '\t': m_out << "\\t";  break;
        case '\a': m_out << "\\a";  break;
        case '\b': m_out << "\\b";  break;
        case '\f': m_out << "\\f";  break;
        case '\v': m_out << "\\v";  break;
        default:
            if (std::isprint(static_cast<unsigned char>(c))) {
                m_out << c;
            }
            else {
                m_out << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(c));
            }
            break;
        }
    }

    // Prints a type with a declarator name (handles complex C declarator syntax)
    void print_declarator(const ast_element* type, const std::string& identifier);

public:
    print_visitor(std::stringstream& out, int indent = 0)
        : m_out(out), m_indent(indent) {}

    // Main entry point - uses accept() for type dispatch to the correct visit() override
    void print(const ast_element* elem) {
        if (!elem) return;
        m_print_requested = true;
        elem->accept(*this);
    }

    // === Type visitors ===
    
    void visit(const void_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "void";
    }

    void visit(const int_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print_int_qualifiers(node.qualifiers());
        switch (node.type_class()) {
        case CHAR_INT_CLASS: m_out << "char"; break;
        case SHORT_INT_CLASS: m_out << "short"; break;
        case INT_INT_CLASS: m_out << "int"; break;
        case LONG_INT_CLASS: m_out << "long"; break;
        }
    }

    void visit(const float_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        switch (node.type_class()) {
        case FLOAT_FLOAT_CLASS: m_out << "float"; break;
        case DOUBLE_FLOAT_CLASS: m_out << "double"; break;
        }
    }

    void visit(const pointer_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.pointee_type());
        m_out << "*";
    }

    void visit(const array_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.element_type());
        m_out << "[";
        if (node.length().has_value() && node.length().value()) {
            print(node.length().value().get());
        }
        m_out << "]";
    }

    void visit(const function_pointer_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.return_type());
        m_out << "(*)(";
        for (size_t i = 0; i < node.parameter_types().size(); ++i) {
            if (i > 0) m_out << ", ";
            print(node.parameter_types()[i].get());
        }
        m_out << ")";
    }

    // === Statement visitors ===

    void visit(const context_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "{\n";
        m_indent++;
        for (const auto& stmt : node.statements()) {
            m_out << indent_str();
            print(stmt.get());
            // Add semicolon for statements that need it
            if (!dynamic_cast<const for_loop*>(stmt.get()) &&
                !dynamic_cast<const while_block*>(stmt.get()) &&
                !dynamic_cast<const do_block*>(stmt.get()) &&
                !dynamic_cast<const if_block*>(stmt.get()) &&
                !dynamic_cast<const if_else_block*>(stmt.get()) &&
                !dynamic_cast<const context_block*>(stmt.get())) {
                m_out << ";";
            }
            m_out << "\n";
        }
        m_indent--;
        m_out << indent_str() << "}";
    }

    void visit(const for_loop& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "for (";
        if (node.initial_statement()) print(node.initial_statement());
        m_out << "; ";
        if (node.condition()) print(node.condition());
        m_out << "; ";
        if (node.increment_statement()) print(node.increment_statement());
        m_out << ") ";
        print(&node.body());
    }

    void visit(const do_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "do ";
        print(&node.body());
        m_out << " while (";
        if (node.condition()) print(node.condition());
        m_out << ");";
    }

    void visit(const while_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "while (";
        if (node.condition()) print(node.condition());
        m_out << ") ";
        print(&node.body());
    }

    void visit(const if_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "if (";
        if (node.condition()) print(node.condition());
        m_out << ") ";
        print(&node.body());
    }

    void visit(const if_else_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "if (";
        if (node.condition()) print(node.condition());
        m_out << ") ";
        print(&node.true_body());
        m_out << " else ";
        print(&node.false_body());
    }

    void visit(const return_statement& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "return";
        if (node.value()) {
            m_out << " ";
            print(node.value());
        }
    }

    void visit(const break_statement& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "break";
        if (node.depth() > 1) {
            m_out << " " << node.depth();
        }
    }

    void visit(const continue_statement& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "continue";
        if (node.depth() > 1) {
            m_out << " " << node.depth();
        }
    }

    // === Expression visitors ===

    void visit(const int_literal& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << node.unsigned_value();
    }

    void visit(const float_literal& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << node.value() << "f";
    }

    void visit(const double_literal& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << node.value();
    }

    void visit(const string_literal& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "\"";
        for (char c : node.value()) {
            escape_char(c);
        }
        m_out << "\"";
    }

    void visit(const variable_reference& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << node.identifier();
    }

    void visit(const get_index& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.pointer());
        m_out << "[";
        print(node.index());
        m_out << "]";
    }

    void visit(const get_property& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.struct_expr());
        m_out << (node.is_pointer_dereference() ? "->" : ".");
        m_out << node.property_name();
    }

    void visit(const set_operator& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.destination());
        m_out << " = ";
        print(node.value());
    }

    void visit(const dereference_operator& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "*";
        print(node.pointer());
    }

    void visit(const get_reference& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "&";
        print(node.item());
    }

    void visit(const arithmetic_operator& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        
        auto it = arithmetic_operator::operator_precedence.find(node.operation());
        int prec = (it != arithmetic_operator::operator_precedence.end()) ? it->second : 0;
        bool need_parens = m_parent_precedence > prec;
        
        if (need_parens) m_out << "(";
        int saved_prec = m_parent_precedence;
        m_parent_precedence = prec;
        print(node.left());
        m_out << " " << token_to_str(node.operation()) << " ";
        print(node.right());
        m_parent_precedence = saved_prec;
        if (need_parens) m_out << ")";
    }

    void visit(const conditional_expression& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        
        const int TERNARY_PREC = 1;
        bool need_parens = m_parent_precedence > TERNARY_PREC;
        
        if (need_parens) m_out << "(";
        int saved_prec = m_parent_precedence;
        m_parent_precedence = TERNARY_PREC;
        m_out << "(";
        print(node.condition());
        m_out << ") ? ";
        print(node.true_expr());
        m_out << " : ";
        print(node.false_expr());
        m_parent_precedence = saved_prec;
        if (need_parens) m_out << ")";
    }

    void visit(const function_call& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.callee());
        m_out << "(";
        for (size_t i = 0; i < node.arguments().size(); ++i) {
            if (i > 0) m_out << ", ";
            print(node.arguments()[i].get());
        }
        m_out << ")";
    }

    void visit(const initializer_list_expression& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "{";
        for (size_t i = 0; i < node.initializers().size(); ++i) {
            if (i > 0) m_out << ", ";
            print(node.initializers()[i].get());
        }
        m_out << "}";
    }

    // === Declaration visitors ===

    void visit(const variable_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        if (node.qualifiers() & CONST_STORAGE_QUALIFIER) m_out << "const ";
        if (node.qualifiers() & VOLATILE_STORAGE_QUALIFIER) m_out << "volatile ";
        if (node.qualifiers() & EXTERN_STORAGE_QUALIFIER) m_out << "extern ";
        if (node.qualifiers() & STATIC_STORAGE_QUALIFIER) m_out << "static ";
        if (node.qualifiers() & REGISTER_STORAGE_QUALIFIER) m_out << "register ";
        print_declarator(node.type(), node.identifier());
        if (node.set_value()) {
            m_out << " = ";
            print(node.set_value());
        }
    }

    void visit(const typedef_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "typedef ";
        print_declarator(node.type(), node.name());
        m_out << ';';
    }

    void visit(const struct_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "struct";
        if (node.struct_name().has_value()) {
            m_out << " " << node.struct_name().value();
        }
        if (node.members().empty()) {
            return;
        }
        m_out << " {\n";
        m_indent++;
        for (const auto& member : node.members()) {
            m_out << indent_str();
            print(&member);
            m_out << ";\n";
        }
        m_indent--;
        m_out << indent_str() << "}";
    }

    void visit(const enum_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "enum";
        if (node.enum_name().has_value()) m_out << " " << node.enum_name().value();
        if (node.enumerators().empty()) {
            return;
        }
        m_out << " {\n";
        for (size_t i = 0; i < node.enumerators().size(); ++i) {
            m_out << "  " << node.enumerators()[i].name;
            if (node.enumerators()[i].value) {
                m_out << " = " << node.enumerators()[i].value.value();
            }
            if (i + 1 < node.enumerators().size()) m_out << ",";
            m_out << "\n";
        }
        m_out << "}";
    }

    void visit(const union_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "union";
        if (node.union_name().has_value()) m_out << " " << node.union_name().value();
        if (node.members().empty()) {
            return;
        }
        m_out << " {\n";
        for (const auto& member : node.members()) {
            m_out << "  ";
            print(member.member_type.get());
            m_out << " " << member.member_name << ";\n";
        }
        m_out << "}";
    }

    void visit(const function_prototype& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.return_type());
        m_out << " " << node.function_name() << "(";
        for (size_t i = 0; i < node.parameters().size(); ++i) {
            print(node.parameters()[i].param_type.get());
            m_out << " " << node.parameters()[i].param_name;
            if (i + 1 < node.parameters().size()) m_out << ", ";
        }
        m_out << ");";
    }

    void visit(const function_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(node.return_type());
        m_out << " " << node.function_name() << "(";
        for (size_t i = 0; i < node.parameters().size(); ++i) {
            print(node.parameters()[i].param_type.get());
            m_out << " " << node.parameters()[i].param_name;
            if (i + 1 < node.parameters().size()) m_out << ", ";
        }
        m_out << ')';
        print(&node.function_body());
    }
};

// Helper for complex C declarator syntax (pointers to arrays, function pointers, etc.)
// This needs special handling because C declarators are "inside-out"
void print_visitor::print_declarator(const ast_element* type, const std::string& identifier) {
    // For complex types, we need to build the declarator recursively
    if (auto* ptr = dynamic_cast<const pointer_type*>(type)) {
        // Pointer to array or function pointer needs parentheses
        if (dynamic_cast<const array_type*>(ptr->pointee_type()) ||
            dynamic_cast<const function_pointer_type*>(ptr->pointee_type())) {
            print_declarator(ptr->pointee_type(), "(*" + identifier + ")");
        } else {
            print(type);
            m_out << ' ' << identifier;
        }
    }
    else if (auto* arr = dynamic_cast<const array_type*>(type)) {
        // Build array suffix and recurse on element type
        std::stringstream length_ss;
        if (arr->length().has_value()) {
            print_visitor len_printer(length_ss, 0);
            len_printer.print(arr->length().value().get());
        }
        print_declarator(arr->element_type(), identifier + "[" + length_ss.str() + "]");
    }
    else if (auto* fptr = dynamic_cast<const function_pointer_type*>(type)) {
        // Build parameter list and recurse on return type
        std::stringstream params_ss;
        print_visitor params_printer(params_ss, 0);
        for (size_t i = 0; i < fptr->parameter_types().size(); ++i) {
            if (i > 0) params_ss << ", ";
            params_printer.print(fptr->parameter_types()[i].get());
        }
        print_declarator(fptr->return_type(), "(*" + identifier + ")(" + params_ss.str() + ")");
    }
    else {
        // Simple types: just print type then identifier
        print(type);
        m_out << ' ' << identifier;
    }
}

// Public API function
namespace michaelcc {
namespace ast {
    std::string to_c_string(const ast_element* elem, int indent) {
        std::stringstream ss;
        print_visitor visitor(ss, indent);
        visitor.print(elem);
        
        // Add semicolon for top-level declarations that need them
        if (dynamic_cast<const variable_declaration*>(elem) ||
            dynamic_cast<const typedef_declaration*>(elem)) {
            ss << ";";
        }
        
        return ss.str();
    }
}
}
