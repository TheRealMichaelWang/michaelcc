#include "linear/registers.hpp"
#include "syntax/tokens.hpp"
#include "syntax/ast.hpp"
#include "logic/typing.hpp"
#include "logic/ir.hpp"
#include "linear/ir.hpp"
#include <cctype>
#include <iomanip>
#include <memory>
#include <unordered_set>
#include <queue>

using namespace michaelcc;
using namespace michaelcc::ast;
using namespace michaelcc::typing;

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
        "restrict",
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
        "inline",
        "tail_call_optimize",
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
        "%",
        "^",
        "&",
        "|",
        "||",
        "&&",
        "<<",
        ">>",
        "==",
        "!=",
        ">",
        "<",
        ">=",
        "<=",
        "?",
        "!",

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

const std::string a_instruction_type_to_str(linear::a_instruction_type type) {
    static const char* a_instruction_type_names[] = {
        "+",                // ADD
        "-",                // SUBTRACT
        "*",                // MULTIPLY
        "s/",               // SIGNED_DIVIDE
        "u/",               // UNSIGNED_DIVIDE
        "s%",               // SIGNED_MODULO
        "u%",               // UNSIGNED_MODULO

        "f+",               // FLOAT_ADD
        "f-",               // FLOAT_SUBTRACT
        "f*",               // FLOAT_MULTIPLY
        "f/",               // FLOAT_DIVIDE
        "f%",               // FLOAT_MODULO

        "<<",               // SHIFT_LEFT
        "s>>",              // SIGNED_SHIFT_RIGHT
        "u>>",              // UNSIGNED_SHIFT_RIGHT
        "&",                // BITWISE_AND
        "|",                // BITWISE_OR
        "^",                // BITWISE_XOR
        "~",                // BITWISE_NOT

        "&&",               // AND
        "||",               // OR
        "^^",               // XOR
        "!",                // NOT

        "==",               // COMPARE_EQUAL
        "!=",               // COMPARE_NOT_EQUAL

        "s<",               // COMPARE_SIGNED_LESS_THAN
        "s<=",              // COMPARE_SIGNED_LESS_THAN_OR_EQUAL
        "u<",               // COMPARE_UNSIGNED_LESS_THAN
        "u<=",              // COMPARE_UNSIGNED_LESS_THAN_OR_EQUAL

        "s>",               // COMPARE_SIGNED_GREATER_THAN
        "s>=",              // COMPARE_SIGNED_GREATER_THAN_OR_EQUAL
        "u>",               // COMPARE_UNSIGNED_GREATER_THAN
        "u>=",              // COMPARE_UNSIGNED_GREATER_THAN_OR_EQUAL

        "f==",              // FLOAT_COMPARE_EQUAL
        "f!=",              // FLOAT_COMPARE_NOT_EQUAL
        "f<",               // FLOAT_COMPARE_LESS_THAN
        "f<=",              // FLOAT_COMPARE_LESS_THAN_OR_EQUAL
        "f>",               // FLOAT_COMPARE_GREATER_THAN
        "f>=",              // FLOAT_COMPARE_GREATER_THAN_OR_EQUAL
    };
    return a_instruction_type_names[type];
}

const std::string u_instruction_type_to_str(linear::u_instruction_type type) {
    static const char* u_instruction_type_names[] = {
        "-",
        "f-",
        "!",
        "~"
    };
    return u_instruction_type_names[type];
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

static void escape_char(std::ostream& out, char c) {
    switch (c) {
    case '\\': out << "\\\\"; break;
    case '"':  out << "\\\""; break;
    case '\n': out << "\\n";  break;
    case '\r': out << "\\r";  break;
    case '\t': out << "\\t";  break;
    case '\a': out << "\\a";  break;
    case '\b': out << "\\b";  break;
    case '\f': out << "\\f";  break;
    case '\v': out << "\\v";  break;
    default:
        if (std::isprint(static_cast<unsigned char>(c))) {
            out << c;
        }
        else {
            out << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(static_cast<unsigned char>(c));
        }
        break;
    }
}

static void print_indent(std::ostream& out, int indent) { 
    for (size_t i = 0; i < indent; i++) { out << '\t'; }
}

class ast_print_visitor : public visitor {
private:
    std::stringstream& m_out;
    int m_indent;
    bool m_print_requested = false;
    int m_parent_precedence = 0;

    void print_type_qualifiers(uint8_t q) {
        if (q & CONST_TYPE_QUALIFIER) m_out << "const ";
        if (q & VOLATILE_TYPE_QUALIFIER) m_out << "volatile ";
        if (q & RESTRICT_TYPE_QUALIFIER) m_out << "restrict ";
    }

    // Prints a type with a declarator name (handles complex C declarator syntax)
    void print_declarator(const ast_element& type, const std::string& identifier);

public:
    ast_print_visitor(std::stringstream& out, int indent = 0)
        : m_out(out), m_indent(indent) {}

    // Main entry point - uses accept() for type dispatch to the correct visit() override
    void print(const ast_element& elem) {
        m_print_requested = true;
        elem.accept(*this);
    }

    // === Type AST visitors ===
    
    void visit(const type_specifier& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        for (size_t i = 0; i < node.type_keywords().size(); ++i) {
            if (i > 0) m_out << " ";
            m_out << token_to_str(node.type_keywords()[i]);
        }
    }

    void visit(const qualified_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print_type_qualifiers(node.qualifiers());
        print(*node.inner_type());
    }

    void visit(const derived_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;

        if (node.is_pointer()) {
            print(*node.inner_type());
            m_out << "*";
        } else {
            print(*node.inner_type());
            m_out << "[";
            if (node.array_size()) {
                print(*node.array_size().value());
            }
            m_out << "]";
        }
    }

    void visit(const function_type& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(*node.return_type());
        m_out << " (*)(";
        for (size_t i = 0; i < node.parameters().size(); ++i) {
            if (i > 0) m_out << ", ";
            print(*node.parameters()[i].param_type);
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
            print_indent(m_out, m_indent);
            print(*stmt);
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
        print_indent(m_out, m_indent);
        m_out << '}';
    }

    void visit(const for_loop& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "for (";
        if (node.initial_statement()) print(*node.initial_statement());
        m_out << "; ";
        if (node.condition()) print(*node.condition());
        m_out << "; ";
        if (node.increment_statement()) print(*node.increment_statement());
        m_out << ") ";
        print(node.body());
    }

    void visit(const do_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "do ";
        print(node.body());
        m_out << " while (";
        if (node.condition()) print(*node.condition());
        m_out << ");";
    }

    void visit(const while_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "while (";
        if (node.condition()) print(*node.condition());
        m_out << ") ";
        print(node.body());
    }

    void visit(const if_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "if (";
        if (node.condition()) print(*node.condition());
        m_out << ") ";
        print(node.body());
    }

    void visit(const if_else_block& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "if (";
        if (node.condition()) print(*node.condition());
        m_out << ") ";
        print(node.true_body());
        m_out << " else ";
        print(node.false_body());
    }

    void visit(const return_statement& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "return";
        if (node.value()) {
            m_out << " ";
            print(*node.value());
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
        m_out << node.value();
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
            escape_char(m_out, c);
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
        print(*node.pointer());
        m_out << "[";
        print(*node.index());
        m_out << "]";
    }

    void visit(const get_property& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(*node.struct_expr());
        m_out << (node.is_pointer_dereference() ? "->" : ".");
        m_out << node.property_name();
    }

    void visit(const set_operator& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(*node.destination());
        m_out << " = ";
        print(*node.value());
    }

    void visit(const dereference_operator& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "*";
        print(*node.pointer());
    }

    void visit(const get_reference& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "&";
        print(*node.item());
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
        print(*node.left());
        m_out << " " << token_to_str(node.operation()) << " ";
        print(*node.right());
        m_parent_precedence = saved_prec;
        if (need_parens) m_out << ")";
    }

    void visit(const unary_operation& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        
        // Unary operators have high precedence
        const int UNARY_PREC = 14;
        bool need_parens = m_parent_precedence > UNARY_PREC;
        
        if (need_parens) m_out << "(";
        m_out << token_to_str(node.get_operator());
        
        // Wrap operand in parens if it's also a unary op to avoid --x looking like predecrement
        bool operand_is_unary = dynamic_cast<const unary_operation*>(node.operand().get()) != nullptr;
        if (operand_is_unary) m_out << "(";
        
        int saved_prec = m_parent_precedence;
        m_parent_precedence = UNARY_PREC;
        print(*node.operand());
        m_parent_precedence = saved_prec;
        
        if (operand_is_unary) m_out << ")";
        if (need_parens) m_out << ")";
    }

    void visit(const cast_expression& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        
        // Cast has high precedence (same as unary)
        const int CAST_PREC = 14;
        bool need_parens = m_parent_precedence > CAST_PREC;
        
        if (need_parens) m_out << "(";
        m_out << "(";
        print(*node.target_type());
        m_out << ")";
        int saved_prec = m_parent_precedence;
        m_parent_precedence = CAST_PREC;
        print(*node.operand());
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
        print(*node.condition());
        m_out << ") ? ";
        print(*node.true_expr());
        m_out << " : ";
        print(*node.false_expr());
        m_parent_precedence = saved_prec;
        if (need_parens) m_out << ")";
    }

    void visit(const function_call& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(*node.callee());
        m_out << "(";
        for (size_t i = 0; i < node.arguments().size(); ++i) {
            if (i > 0) m_out << ", ";
            print(*node.arguments()[i]);
        }
        m_out << ")";
    }

    void visit(const initializer_list_expression& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "{";
        for (size_t i = 0; i < node.initializers().size(); ++i) {
            if (i > 0) m_out << ", ";
            print(*node.initializers()[i]);
        }
        m_out << "}";
    }

    // === Declaration visitors ===

    void visit(const variable_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        uint8_t storage = node.qualifiers() & 0x0F;
        uint8_t type_quals = (node.qualifiers() >> 4) & 0x0F;
        if (type_quals & CONST_TYPE_QUALIFIER) m_out << "const ";
        if (type_quals & VOLATILE_TYPE_QUALIFIER) m_out << "volatile ";
        if (storage & EXTERN_STORAGE_CLASS) m_out << "extern ";
        if (storage & STATIC_STORAGE_CLASS) m_out << "static ";
        if (storage & REGISTER_STORAGE_CLASS) m_out << "register ";
        print_declarator(*node.type(), node.identifier());
        if (node.set_value()) {
            m_out << " = ";
            print(*node.set_value().value());
        }
    }

    void visit(const typedef_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "typedef ";
        print_declarator(*node.type(), node.name());
        m_out << ';';
    }

    void visit(const struct_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        m_out << "struct";
        if (node.struct_name().has_value()) {
            m_out << " " << node.struct_name().value();
        }
        if (node.fields().empty()) {
            return;
        }
        m_out << " {\n";
        m_indent++;
        for (const auto& field : node.fields()) {
            print_indent(m_out, m_indent);
            print(field);
            m_out << ";\n";
        }
        m_indent--;
        print_indent(m_out, m_indent);
        m_out << '}';
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
            print(*member.member_type);
            m_out << " " << member.member_name << ";\n";
        }
        m_out << "}";
    }

    void visit(const function_prototype& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(*node.return_type());
        m_out << " " << node.function_name() << "(";
        for (size_t i = 0; i < node.parameters().size(); ++i) {
            print(*node.parameters()[i].param_type);
            m_out << " " << node.parameters()[i].param_name;
            if (i + 1 < node.parameters().size()) m_out << ", ";
        }
        m_out << ");";
    }

    void visit(const function_declaration& node) override {
        if (!m_print_requested) return;
        m_print_requested = false;
        print(*node.return_type());
        m_out << " " << node.function_name() << "(";
        for (size_t i = 0; i < node.parameters().size(); ++i) {
            print(*node.parameters()[i].param_type);
            m_out << " " << node.parameters()[i].param_name;
            if (i + 1 < node.parameters().size()) m_out << ", ";
        }
        m_out << ')';
        print(node.function_body());
    }
};

// Helper for complex C declarator syntax (pointers to arrays, function pointers, etc.)
// This needs special handling because C declarators are "inside-out"
void ast_print_visitor::print_declarator(const ast_element& type, const std::string& identifier) {
    if (auto* derived = dynamic_cast<const derived_type*>(&type)) {
        if (derived->is_pointer()) {
            // Check if pointee is array or function - needs parentheses
            const ast_element* inner = derived->inner_type().get();
            if (auto* inner_derived = dynamic_cast<const derived_type*>(inner)) {
                if (inner_derived->is_array()) {
                    print_declarator(*inner, "(*" + identifier + ")");
                    return;
                }
            }
            if (dynamic_cast<const function_type*>(inner)) {
                print_declarator(*inner, "(*" + identifier + ")");
                return;
            }
            // Simple pointer
            print(type);
            m_out << ' ' << identifier;
        } else {
            // Array - build suffix and recurse on element type
            std::stringstream length_ss;
            if (derived->array_size()) {
                ast_print_visitor len_printer(length_ss, 0);
                len_printer.print(*derived->array_size().value());
            }
            print_declarator(*derived->inner_type(), identifier + "[" + length_ss.str() + "]");
        }
    }
    else if (auto* func = dynamic_cast<const function_type*>(&type)) {
        // Build parameter list and recurse on return type
        std::stringstream params_ss;
        ast_print_visitor params_printer(params_ss, 0);
        for (size_t i = 0; i < func->parameters().size(); ++i) {
            if (i > 0) params_ss << ", ";
            params_printer.print(*func->parameters()[i].param_type);
        }
        print_declarator(*func->return_type(), identifier + "(" + params_ss.str() + ")");
    }
    else if (auto* qualified = dynamic_cast<const qualified_type*>(&type)) {
        // Print qualifiers then recurse
        print_type_qualifiers(qualified->qualifiers());
        print_declarator(*qualified->inner_type(), identifier);
    }
    else {
        // Simple types (type_specifier, struct_declaration, etc.)
        print(type);
        m_out << ' ' << identifier;
    }
}

// Public API function
namespace michaelcc {
namespace ast {
    std::string to_c_string(const ast_element& elem, int indent) {
        std::stringstream ss;
        ast_print_visitor visitor(ss, indent);
        visitor.print(elem);
        
        // Add semicolon for top-level declarations that need them
        if (dynamic_cast<const variable_declaration*>(&elem) ||
            dynamic_cast<const typedef_declaration*>(&elem)) {
            ss << ";";
        }
        
        return ss.str();
    }
}
}

class logical_expression_printer : public logic::const_expression_dispatcher<void> {
private:
    std::ostream& m_out;
    const logic::translation_unit& m_unit;
    int m_indent;

public:
    logical_expression_printer(std::ostream& out, const logic::translation_unit& unit, int indent = 0) 
    : m_out(out), m_unit(unit), m_indent(indent) {}

protected:
    void dispatch(const logic::integer_constant& node) override {
        m_out << node.value();
    }

    void dispatch(const logic::floating_constant& node) override {
        m_out << node.value();
    }
    
    void dispatch(const logic::string_constant& node) override {
        m_out << "\"";
        for (char c : m_unit.strings().at(node.index())) {
            escape_char(m_out, c);
        }
        m_out << "\"";
    }

    void dispatch(const logic::enumerator_literal& node) override {
        m_out << node.enumerator().name;
    }

    void dispatch(const logic::variable_reference& node) override {
        m_out << node.get_variable()->name();
    }

    void dispatch(const logic::function_reference& node) override {
        m_out << node.get_function()->name();
    }

    void dispatch(const logic::increment_operator& node) override {
        std::visit(overloaded{
            [this](const std::unique_ptr<logic::expression>& expr) { m_out << "*("; (*this)(*expr); m_out << ")"; },
            [this](const std::shared_ptr<logic::variable>& var) { m_out << var->name(); }
        }, node.destination());

        m_out << ' ' << token_to_str(node.get_operator()) << ' ';

        if (node.increment_amount().has_value()) {
            m_out << ' ';
            (*this)(*node.increment_amount().value());
        }
    }

    void dispatch(const logic::arithmetic_operator& node) override {
        m_out << '(';
        (*this)(*node.left());
        m_out << ' ' << token_to_str(node.get_operator()) << ' ';
        (*this)(*node.right());
        m_out << ')';
    }

    void dispatch(const logic::unary_operation& node) override {
        m_out << '(' << token_to_str(node.get_operator());
        (*this)(*node.operand());
        m_out << ')';
    }

    void dispatch(const logic::type_cast& node) override {
        m_out << '(';
        m_out << "(";
        m_out << node.get_type().to_string();
        m_out << ") ";
        (*this)(*node.operand());
        m_out << ')';
    }
    
    void dispatch(const logic::address_of& node) override {
        m_out << "&";
        std::visit(overloaded{
            [this](const std::unique_ptr<logic::array_index>& index) { (*this)(*index); },
            [this](const std::unique_ptr<logic::member_access>& access) { (*this)(*access); },
            [this](const std::shared_ptr<logic::variable>& var) { m_out << var->name(); }
        }, node.operand());
    }

    void dispatch(const logic::dereference& node) override {
        m_out << '*';
        (*this)(*node.operand());
    }

    void dispatch(const logic::array_index& node) override {
        (*this)(*node.base());
        m_out << '[';
        (*this)(*node.index());
        m_out << ']';
    }

    void dispatch(const logic::member_access& node) override {
        (*this)(*node.base());
        m_out << (node.is_dereference() ? "->" : ".");
        m_out << node.member().name;
    }
    
    void dispatch(const logic::array_initializer& node) override {
        m_out << "{";
        for (size_t i = 0; i < node.initializers().size(); ++i) {
            if (i > 0) m_out << ", ";
            (*this)(*node.initializers()[i]);
        }
        m_out << "}";
    }

    void dispatch(const logic::allocate_array& node) override {
        m_out << "new " << node.get_type().to_string();

        for (const auto& dimension : node.dimensions()) {
            m_out << '[';
            (*this)(*dimension);
            m_out << ']';
        }

        if (node.fill_value()) {
            m_out << '(';
            (*this)(*node.fill_value());
            m_out << ')';
        }
    }

    void dispatch(const logic::struct_initializer& node) override {
        m_out << node.get_type().to_string();
        m_out << " {";
        for (const auto& initializer : node.initializers()) {
            m_out << initializer.member_name << " = ";
            (*this)(*initializer.initializer);
            m_out << ", ";
        }
        m_out << "}";
    }

    void dispatch(const logic::union_initializer& node) override {
        m_out << node.get_type().to_string();
        m_out << " {" << node.target_member().name << " = ";
        (*this)(*node.initializer());
        m_out << "}";
    }

    void dispatch(const logic::function_call& node) override {
        std::visit(overloaded{
            [this](const std::shared_ptr<logic::function_definition>& function) { m_out << function->name(); },
            [this](const std::unique_ptr<logic::expression>& expression) { (*this)(*expression); }
        }, node.callee());
        m_out << "(";
        bool first = true;
        for (const auto& argument : node.arguments()) {
            if (!first) m_out << ", ";
            (*this)(*argument);
            first = false;
        }
        m_out << ")";
    }

    void dispatch(const logic::conditional_expression& node) override {
        m_out << "(";
        (*this)(*node.condition());
        m_out << " ? ";
        (*this)(*node.then_expression());
        m_out << " : ";
        (*this)(*node.else_expression());
        m_out << ')';
    }

    void dispatch(const logic::set_address& node) override {
        m_out << "*(";
        (*this)(*node.destination());
        m_out << ") = ";
        (*this)(*node.value());
    }

    void dispatch(const logic::set_variable& node) override {
        m_out << node.variable()->name() << " = ";
        (*this)(*node.value());
    }

    void dispatch(const logic::compound_expression& node) override;
};

class logical_statement_printer : public logic::const_statement_dispatcher<void> {
private:
    std::ostream& m_out;
    const logic::translation_unit& m_unit;
    logical_expression_printer m_expression_printer;
    int m_indent;

public:
    logical_statement_printer(std::ostream& out, const logic::translation_unit& unit, int indent = 0) 
    : m_out(out), m_unit(unit), m_expression_printer(out, unit), m_indent(indent) {}

    void begin_indent() { m_indent++; }
    void end_indent() { m_indent--; }

protected:
    void dispatch(const logic::expression_statement& node) override {
        print_indent(m_out, m_indent);
        m_expression_printer(*node.expression());
        m_out << ";\n";
    }

    void dispatch(const logic::variable_declaration& node) override {
        print_indent(m_out, m_indent);
        m_out << node.variable()->get_type().to_string() << " " << node.variable()->name();
        m_out << " = ";
        m_expression_printer(*node.initializer());
        m_out << ";\n";
    }

    void dispatch(const logic::return_statement& node) override {
        print_indent(m_out, m_indent);
        m_out << "return ";
        if (node.value()) {
            m_expression_printer(*node.value());
        }
        m_out << ";\n";
    }
    
    void dispatch(const logic::if_statement& node) override {
        print_indent(m_out, m_indent);
        m_out << "if ";
        m_expression_printer(*node.condition());
        m_out << " {\n";
        m_indent++;
        for (const auto& statement : node.then_body()->statements()) {
            (*this)(*statement);
        }
        m_indent--;
        print_indent(m_out, m_indent);
        m_out << '}';

        if (node.else_body()) {
            m_out << " else {\n";
            m_indent++;
            for (const auto& statement : node.else_body()->statements()) {
                (*this)(*statement);
            }
            m_indent--;
            print_indent(m_out, m_indent);
            m_out << '}';
        }
        m_out << '\n';
    }

    void dispatch(const logic::break_statement& node) override {
        print_indent(m_out, m_indent);
        m_out << "break";
        if (node.loop_depth() > 1) {
            m_out << " " << node.loop_depth();
        }
        m_out << ";\n";
    }
    
    void dispatch(const logic::loop_statement& node) override {
        print_indent(m_out, m_indent);

        if (node.check_condition_first()) {
            m_out << "while ";
            m_expression_printer(*node.condition());
            m_out << " {\n";
        }
        else {
            m_out << "do {\n";
        }
        m_indent++;
        for (const auto& statement : node.body()->statements()) {
            (*this)(*statement);
        }
        m_indent--;
        print_indent(m_out, m_indent);
        m_out << "}";

        if (!node.check_condition_first()) {
            m_out << " while ";
            m_expression_printer(*node.condition());
            m_out << ";";
        }

        m_out << '\n';
    }

    void dispatch(const logic::continue_statement& node) override {
        print_indent(m_out, m_indent);
        m_out << "continue";
        if (node.loop_depth() > 1) {
            m_out << " " << node.loop_depth();
        }
        m_out << ";\n";
    }

    void dispatch(const logic::statement_block& node) override {
        print_indent(m_out, m_indent);
        m_out << "{\n";
        m_indent++;
        for (const auto& statement : node.control_block()->statements()) {
            (*this)(*statement);
        }
        m_indent--;
        print_indent(m_out, m_indent);
        m_out << "}\n";
    }
};

void logical_expression_printer::dispatch(const logic::compound_expression& node) {
    m_out << "({";
    logical_statement_printer statement_printer(m_out, m_unit, m_indent + 1);
    for (const auto& statement : node.control_block()->statements()) {
        statement_printer(*statement);
    }
    print_indent(m_out, m_indent);
    m_out << "})";
}

class linear_print_visitor : linear::const_visitor {
private:
    std::ostream& m_out;
    size_t m_indent;
    std::vector<size_t> subsequent_block_ids;
    const linear::translation_unit& m_unit;
    const platform_info& m_platform_info;
    
    void print_virtual_register(const linear::virtual_register& reg, bool include_size=false, bool is_setting=false) {
        m_out << '%' << reg.id;
        if (include_size) {
            m_out << "(";
            switch (reg.reg_class) {
                case linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER: m_out << "int, "; break;
                case linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT: m_out << "float, "; break;
                default: throw std::runtime_error("Invalid register class");
            }
            m_out << static_cast<size_t>(reg.reg_size) << " bits";

            if (is_setting) {
                
                if (m_unit.vreg_colors.contains(reg)) {
                    auto register_info = m_platform_info.get_register_info(m_unit.vreg_colors.at(reg));
                    m_out << ", register " << register_info.name;
                }
                if (m_unit.cannot_spill_vregs.contains(reg)) {
                    m_out << ", register required";
                }
            }

            m_out << ")";
        }
    }

    void print_register_word(const linear::register_word& word, const linear::word_size size, const linear::register_class reg_class) {
        if (reg_class == linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER) {
            m_out << 'u';
            switch (size) {
                case linear::word_size::MICHAELCC_WORD_SIZE_BYTE: m_out << word.ubyte; break;
                case linear::word_size::MICHAELCC_WORD_SIZE_UINT16: m_out << word.uint16; break;
                case linear::word_size::MICHAELCC_WORD_SIZE_UINT32: m_out << word.uint32; break;
                case linear::word_size::MICHAELCC_WORD_SIZE_UINT64: m_out << word.uint64; break;
                default: throw std::runtime_error("Invalid word size");
            }
        } else {
            m_out << 'f';
            switch (size) {
                case linear::word_size::MICHAELCC_WORD_SIZE_UINT32: m_out << word.float32; break;
                case linear::word_size::MICHAELCC_WORD_SIZE_UINT64: m_out << word.float64; break;
                default: throw std::runtime_error("Invalid word size");
            }
        }
    }

public:
    linear_print_visitor(std::ostream& out, const linear::translation_unit& unit, const platform_info& platform_info, int indent = 0) 
    : m_out(out), m_unit(unit), m_platform_info(platform_info), m_indent(indent) {}

    std::vector<size_t> print_basic_block(const linear::basic_block& node, std::optional<std::string> label = std::nullopt) {
        print_indent(m_out, m_indent);
        if (label.has_value()) {
            m_out << label.value() << "(id=" << node.id();
        }
        else {
            m_out << "basic_block(id=" << node.id();
        }
        if (node.immediate_dominator_block_id().has_value()) {
            m_out << ", idom=" << node.immediate_dominator_block_id().value();
        }
        m_out << ", pred=[";
        bool first = true;
        for (const auto& predecessor_block_id : node.predecessor_block_ids()) {
            if (!first) {
                m_out << ", ";
            }
            m_out << predecessor_block_id;
            first = false;
        }
        m_out << "], succ=[";
        first = true;
        for (const auto& successor_block_id : node.successor_block_ids()) {
            if (!first) {
                m_out << ", ";
            }
            m_out << successor_block_id;
            first = false;
        }
        m_out << "], dom=[";
        first = true;
        for (const auto& dominated_block_id : node.immediately_dominated_block_ids()) {
            if (!first) {
                m_out << ", ";
            }
            m_out << dominated_block_id;
            first = false;
        }
        m_out << "]):\n";
        m_indent++;
        for (const auto& instruction : node.instructions()) {
            (*this)(*instruction);
        }
        m_indent--;

        return subsequent_block_ids;
    }

protected:
    void dispatch(const linear::a_instruction& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = ";
        print_virtual_register(node.operand_a());
        m_out << " " << a_instruction_type_to_str(node.type()) << " ";
        print_virtual_register(node.operand_b());
        m_out << "\n";
    }

    void dispatch(const linear::a2_instruction& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = ";
        print_virtual_register(node.operand_a());
        m_out << " " << a_instruction_type_to_str(node.type()) << " " << node.constant() << " ";
        m_out << "\n";
    }

    void dispatch(const linear::u_instruction& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = " << u_instruction_type_to_str(node.type());
        print_virtual_register(node.operand());
        m_out << "\n";
    }

    void dispatch(const linear::c_instruction& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        
        m_out << " =";
        switch (node.type()) {
            case linear::MICHAELCC_LINEAR_C_FLOAT64_TO_SIGNED_INT64: m_out << "f64_to_i64"; break;
            case linear::MICHAELCC_LINEAR_C_FLOAT64_TO_UNSIGNED_INT64: m_out << "f64_to_u64"; break;
            case linear::MICHAELCC_LINEAR_C_FLOAT32_TO_SIGNED_INT32: m_out << "f32_to_i32"; break;
            case linear::MICHAELCC_LINEAR_C_FLOAT32_TO_UNSIGNED_INT32: m_out << "f32_to_u32"; break;
            case linear::MICHAELCC_LINEAR_C_SIGNED_INT64_TO_FLOAT64: m_out << "i64_to_f64"; break;
            case linear::MICHAELCC_LINEAR_C_UNSIGNED_INT64_TO_FLOAT64: m_out << "u64_to_f64"; break;
            case linear::MICHAELCC_LINEAR_C_SIGNED_INT32_TO_FLOAT32: m_out << "i32_to_f32"; break;
            case linear::MICHAELCC_LINEAR_C_UNSIGNED_INT32_TO_FLOAT32: m_out << "u32_to_f32"; break;
            case linear::MICHAELCC_LINEAR_C_FLOAT32_TO_FLOAT64: m_out << "f32_to_f64"; break;
            case linear::MICHAELCC_LINEAR_C_FLOAT64_TO_FLOAT32: m_out << "f64_to_f32"; break;
            case linear::MICHAELCC_LINEAR_C_SEXT_OR_TRUNC: 
                m_out << "sext_or_trunc"; break;
            case linear::MICHAELCC_LINEAR_C_ZEXT_OR_TRUNC: 
                m_out << "zext_or_trunc"; break;
            case linear::MICHAELCC_LINEAR_C_COPY_INIT: m_out << "copy_init"; break;
            default: throw std::runtime_error("Invalid c instruction type");
        }

        m_out << ' ';

        print_virtual_register(node.source(), (node.type() == linear::MICHAELCC_LINEAR_C_SEXT_OR_TRUNC || node.type() == linear::MICHAELCC_LINEAR_C_ZEXT_OR_TRUNC));
        m_out << "\n";
    }

    void dispatch(const linear::init_register& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = ";
        print_register_word(node.value(), node.destination().reg_size, node.destination().reg_class);
        m_out << "\n";
    }

    void dispatch(const linear::load_memory& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = *(";
        print_virtual_register(node.source_address());
        m_out << " + " << node.offset() << ")\n";
    }

    void dispatch(const linear::store_memory& node) override {
        print_indent(m_out, m_indent);
        m_out << "*(";
        print_virtual_register(node.destination_address(), true, true);
        m_out << " + " << node.offset() << ") = ";
        print_virtual_register(node.value(), true);
        m_out << "\n";
    }

    void dispatch(const linear::alloca_instruction& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = alloca(size=" << node.size_bytes() << " bytes, alignment=" << node.alignment() << " bytes)\n";
    }

    void dispatch(const linear::valloca_instruction& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = valloca(size=";
        print_virtual_register(node.size());
        m_out << " bytes, alignment=" << node.alignment() << " bytes)\n";
    }

    void dispatch(const linear::branch& node) override {
        print_indent(m_out, m_indent);
        m_out << "branch(block" << node.next_block_id() << ")\n";
        subsequent_block_ids.push_back(node.next_block_id());
    }

    void dispatch(const linear::branch_condition& node) override {
        print_indent(m_out, m_indent);
        m_out << "branch_condition(condition=";
        print_virtual_register(node.condition(), true, true);
        m_out << ", true_block=" << node.if_true_block_id() << ", false_block=" << node.if_false_block_id() << ", is_loop=" << node.is_loop() << ")\n";
        subsequent_block_ids.push_back(node.if_true_block_id());
        subsequent_block_ids.push_back(node.if_false_block_id());
    }

    void dispatch(const linear::function_call& node) override {
        print_indent(m_out, m_indent);
        if (node.destination().has_value()) {
            print_virtual_register(node.destination().value(), true, true);
            m_out << " = ";
        }
        m_out << "function_call(callee=";
        std::visit(overloaded{
            [this](const std::string& label) { m_out << label; },
            [this](const linear::virtual_register& reg) { print_virtual_register(reg, true); }
        }, node.callee());
        m_out << ", arguments=" << node.argument_count() << ")\n";
    }

    void dispatch(const linear::function_return& node) override {
        print_indent(m_out, m_indent);
        m_out << "return\n";
    }

    void dispatch(const linear::push_function_argument& node) override {
        print_indent(m_out, m_indent);
        m_out << "push_fn_arg(arg";
        if (node.argument().pass_via_stack()) {
            m_out << "_addr";
        }
        m_out << '=';
        print_virtual_register(node.value(), true, false);
        m_out << ")\n";
    }

    void dispatch(const linear::load_parameter& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = load_parameter";
        if (node.parameter().pass_via_stack()) {
            m_out << "_addr";
        }
        m_out << "(parameter=" << node.parameter().name << ")\n";
    }

    void dispatch(const linear::phi_instruction& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = phi(";
        bool first = true;
        for (const auto& value : node.values()) {
            if (!first) {
                m_out << ", ";
            }
            print_virtual_register(value.vreg, true);

            m_out << " from block " << value.block_id;

            first = false;
        }
        m_out << ")\n";
    }

    void dispatch(const linear::load_effective_address& node) override {
        print_indent(m_out, m_indent);
        print_virtual_register(node.destination(), true, true);
        m_out << " = load_effective_address(label=" << node.label() << ")\n";
    }
};

namespace michaelcc {
namespace logic {
    std::string to_tree_string(const translation_unit& unit) {
        std::stringstream ss;

        logical_statement_printer statement_printer(ss, unit, 0);
        for (const auto& static_variable : unit.static_variable_declarations()) {
            statement_printer(static_variable);
        }

        for (const auto& sym : unit.global_context()->symbols()) {
            auto* func = dynamic_cast<logic::function_definition*>(sym.get());
            if (!func) { continue; }

            if (func->should_inline()) {
                ss << "inline ";
            }
            if (func->should_tail_call_optimize()) {
                ss << "tail_call_optimize ";
            }
            ss << func->return_type().to_string() << " " << func->name() << "(";
            bool first = true;
            for (const auto& param : func->parameters()) {
                if (!first) {
                    ss << ", ";
                }
                ss << param->get_type().to_string() << " " << param->name();
            }
            ss << ") {\n";

            statement_printer.begin_indent();
            for (const auto& statement : func->statements()) {
                statement_printer(*statement);
            }
            statement_printer.end_indent();
            ss << "}\n";
        }

        return ss.str();
    }
}
}

namespace michaelcc {
namespace linear {
    std::string print_linear_ir(const translation_unit& unit) {
        std::stringstream ss;
        
        std::unordered_set<size_t> printed_block_ids;
        std::queue<size_t> block_queue;
        
        for (const auto& func : unit.function_definitions) {
            block_queue.push(func->entry_block_id());
            std::optional<std::string> label = func->name();
            while (!block_queue.empty()) {
                size_t block_id = block_queue.front();
                block_queue.pop();

                if (printed_block_ids.contains(block_id)) continue;
                
                printed_block_ids.insert(block_id);
                linear_print_visitor visitor(ss, unit, unit.platform_info, 0);
                auto subsequent_block_ids = visitor.print_basic_block(unit.blocks.at(block_id), label);
                if (label.has_value()) {
                    label = std::nullopt;
                }

                for (const auto& subsequent_block_id : subsequent_block_ids) {
                    if (printed_block_ids.contains(subsequent_block_id)) continue;
                    block_queue.push(subsequent_block_id);
                }
            }
        }

        return ss.str();
    }
}
}