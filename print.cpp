#include "linear/registers.hpp"
#include "syntax/tokens.hpp"
#include "syntax/ast.hpp"
#include "logic/typing.hpp"
#include "logic/ir.hpp"
#include "linear/ir.hpp"
#include <cctype>
#include <iomanip>
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
        "+",
        "-",
        "*",
        "/",
        "%",
        "<<",
        ">>",
        "&",
        "|",
        "^",
        "~",
        "&&",
        "||",
        "^^",
        "!",
        "==",
        "!=",
        "<",
        "<=",
        ">",
        ">="
    };
    return a_instruction_type_names[type];
}

const std::string u_instruction_type_to_str(linear::u_instruction_type type) {
    static const char* u_instruction_type_names[] = {
        "-",
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

class ast_print_visitor : public visitor {
private:
    std::stringstream& m_out;
    int m_indent;
    bool m_print_requested = false;
    int m_parent_precedence = 0;

    std::string indent_str() const {
        return std::string(m_indent * 2, ' ');
    }

    void print_type_qualifiers(uint8_t q) {
        if (q & CONST_TYPE_QUALIFIER) m_out << "const ";
        if (q & VOLATILE_TYPE_QUALIFIER) m_out << "volatile ";
        if (q & RESTRICT_TYPE_QUALIFIER) m_out << "restrict ";
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
            m_out << indent_str();
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
        m_out << indent_str() << "}";
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
            m_out << indent_str();
            print(field);
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

// Simple tree printer for logical IR using stack-based indent tracking
class logical_print_visitor : public logic::const_visitor {
private:
    std::ostream& m_out;
    int m_indent = 0;
    std::vector<int> m_child_stack;

    void print_indent() { 
        for (int i = 0; i < m_indent; i++) { m_out << "\t"; }
    }

    // Call before printing: decrement parent's expected child count
    void before_print() {
        if (!m_child_stack.empty()) {
            m_child_stack.back()--;
        }
    }

    // Call after printing: pop completed parents, then push our child count
    void after_print(int child_count) {
        // Pop any parents whose children are all visited
        while (!m_child_stack.empty() && m_child_stack.back() == 0) {
            m_child_stack.pop_back();
            m_indent--;
        }
        // Push our child count and indent if we have children
        if (child_count > 0) {
            m_child_stack.push_back(child_count);
            m_indent++;
        }
    }

public:
    logical_print_visitor(std::ostream& out) : m_out(out) {}

    // translation_unit: children = number of global symbols (vars + funcs)
    void visit(const logic::translation_unit& node) override {
        before_print();
        print_indent();
        m_out << "translation_unit\n";
        // Count how many symbols will be visited (see translation_unit::accept)
        int child_count = 0;
        for (const auto& sym : node.global_symbols()) {
            if (dynamic_cast<logic::variable*>(sym.get()) ||
                dynamic_cast<logic::function_definition*>(sym.get())) {
                child_count++;
            }
        }
        after_print(child_count);
    }

    // variable: 0 children
    void visit(const logic::variable& node) override {
        before_print();
        print_indent();
        m_out << "variable: " << node.name() << (node.is_global() ? " (global)" : " (local)") << "\n";
        after_print(0);
    }

    // function_definition: params.size() + 1 (for control_block base)
    void visit(const logic::function_definition& node) override {
        before_print();
        print_indent();
        m_out << "function_definition: " << node.name() << "\n";
        int child_count = static_cast<int>(node.parameters().size()) + 1;
        after_print(child_count);
    }

    // control_block: statements.size()
    void visit(const logic::control_block& node) override {
        before_print();
        print_indent();
        m_out << "control_block\n";
        after_print(static_cast<int>(node.statements().size()));
    }

    // statement_block: 1 child (control_block)
    void visit(const logic::statement_block& node) override {
        before_print();
        print_indent();
        m_out << "statement_block\n";
        after_print(1);
    }

    // integer_constant: 0 children
    void visit(const logic::integer_constant& node) override {
        before_print();
        print_indent();
        m_out << "integer_constant: " << node.value() << "\n";
        after_print(0);
    }

    // floating_constant: 0 children
    void visit(const logic::floating_constant& node) override {
        before_print();
        print_indent();
        m_out << "floating_constant: " << node.value() << "\n";
        after_print(0);
    }

    // string_constant: 0 children
    void visit(const logic::string_constant& node) override {
        before_print();
        print_indent();
        m_out << "string_constant: [index " << node.index() << "]\n";
        after_print(0);
    }

    // variable_reference: 0 children (doesn't visit the variable)
    void visit(const logic::variable_reference& node) override {
        before_print();
        print_indent();
        m_out << "variable_reference: " << node.get_variable()->name() << "\n";
        after_print(0);
    }

    // function_reference: 0 children
    void visit(const logic::function_reference& node) override {
        before_print();
        print_indent();
        m_out << "function_reference: " << node.get_function()->name() << "\n";
        after_print(0);
    }

    // var_increment_operator: 0 children (doesn't visit destination/amount)
    void visit(const logic::increment_operator& node) override {
        before_print();
        print_indent();
        m_out << "increment_operator\n";
        after_print(0);
    }

    // arithmetic_operator: 2 children (left, right)
    void visit(const logic::arithmetic_operator& node) override {
        before_print();
        print_indent();
        m_out << "arithmetic_operator: " << token_to_str(node.get_operator()) << "\n";
        after_print(2);
    }

    // unary_operation: 1 child (operand)
    void visit(const logic::unary_operation& node) override {
        before_print();
        print_indent();
        m_out << "unary_operation: " << token_to_str(node.get_operator()) << "\n";
        after_print(1);
    }

    // type_cast: 1 child (operand)
    void visit(const logic::type_cast& node) override {
        before_print();
        print_indent();
        m_out << "type_cast\n";
        after_print(1);
    }

    // address_of: 1 child (operand)
    void visit(const logic::address_of& node) override {
        before_print();
        print_indent();
        m_out << "address_of\n";
        after_print(1);
    }

    // dereference: 1 child (operand)
    void visit(const logic::dereference& node) override {
        before_print();
        print_indent();
        m_out << "dereference\n";
        after_print(1);
    }

    // member_access: 1 child (base)
    void visit(const logic::member_access& node) override {
        before_print();
        print_indent();
        m_out << "member_access: [field " << node.member().name << "]\n";
        after_print(1);
    }

    // array_index: 2 children (base, index)
    void visit(const logic::array_index& node) override {
        before_print();
        print_indent();
        m_out << "array_index\n";
        after_print(2);
    }

    // array_initializer: initializers.size() children
    void visit(const logic::array_initializer& node) override {
        before_print();
        print_indent();
        m_out << "array_initializer\n";
        after_print(static_cast<int>(node.initializers().size()));
    }

    // allocate_array: dimensions.size() children
    void visit(const logic::allocate_array& node) override {
        before_print();
        print_indent();
        m_out << "allocate_array\n";
        after_print(static_cast<int>(node.dimensions().size()));
    }

    // struct_initializer: initializers.size() children
    void visit(const logic::struct_initializer& node) override {
        before_print();
        print_indent();
        m_out << "struct_initializer\n";
        after_print(static_cast<int>(node.initializers().size()));
    }

    // union_initializer: 1 child (initializer)
    void visit(const logic::union_initializer& node) override {
        before_print();
        print_indent();
        m_out << "union_initializer\n";
        after_print(1);
    }

    // function_call: 1 + arguments.size() (callee + args)
    void visit(const logic::function_call& node) override {
        before_print();
        print_indent();
        m_out << "function_call\n";
        after_print(1 + static_cast<int>(node.arguments().size()));
    }

    // conditional_expression: 3 children (condition, then, else)
    void visit(const logic::conditional_expression& node) override {
        before_print();
        print_indent();
        m_out << "conditional_expression\n";
        after_print(3);
    }

    // set_address: 2 children (destination, value)
    void visit(const logic::set_address& node) override {
        before_print();
        print_indent();
        m_out << "set_address\n";
        after_print(2);
    }

    // set_variable: 2 children (variable, value)
    void visit(const logic::set_variable& node) override {
        before_print();
        print_indent();
        m_out << "set_variable\n";
        after_print(2);
    }

    void visit(const logic::compound_expression& node) override {
        before_print();
        print_indent();
        m_out << "compound_expression\n";
        after_print(1);
    }

    // expression_statement: 1 child (expression)
    void visit(const logic::expression_statement& node) override {
        before_print();
        print_indent();
        m_out << "expression_statement\n";
        after_print(1);
    }

    // local_declaration: 1 + (initializer ? 1 : 0)
    void visit(const logic::variable_declaration& node) override {
        before_print();
        print_indent();
        m_out << "local_declaration\n";
        after_print(1 + (node.initializer() ? 1 : 0));
    }

    // return_statement: (value ? 1 : 0)
    void visit(const logic::return_statement& node) override {
        before_print();
        print_indent();
        if (node.is_compound_return()) {
            m_out << "compound_";
        }
        m_out << "return_statement\n";
        after_print(node.value() ? 1 : 0);
    }

    // if_statement: 2 + (else_body ? 1 : 0)
    void visit(const logic::if_statement& node) override {
        before_print();
        print_indent();
        m_out << "if_statement\n";
        after_print(2 + (node.else_body() ? 1 : 0));
    }

    // loop_statement: 2 children (condition, body)
    void visit(const logic::loop_statement& node) override {
        before_print();
        print_indent();
        m_out << "loop_statement" << (node.check_condition_first() ? " (while)" : " (do-while)") << "\n";
        after_print(2);
    }

    // break_statement: 0 children
    void visit(const logic::break_statement& node) override {
        before_print();
        print_indent();
        m_out << "break_statement\n";
        after_print(0);
    }

    // continue_statement: 0 children
    void visit(const logic::continue_statement& node) override {
        before_print();
        print_indent();
        m_out << "continue_statement\n";
        after_print(0);
    }
};

class linear_print_visitor : linear::const_visitor {
private:
    std::ostream& m_out;
    size_t m_indent;
    std::vector<size_t> subsequent_block_ids;
    const linear::register_allocator& m_register_allocator;
    const platform_info& m_platform_info;
    
    void print_virtual_register(const linear::virtual_register& reg, bool include_size=false) {
        m_out << '%' << reg.id;
        if (include_size) {
            m_out << "(";
            m_out << static_cast<size_t>(reg.reg_size) << " bits";

            auto alloc_info = m_register_allocator.get_alloc_information(reg);
            if (alloc_info.register_id) {
                auto register_info = m_platform_info.get_register_info(alloc_info.register_id.value());
                m_out << ", register " << register_info.name;
            }

            m_out << ")";
        }
    }

    void print_register_word(const linear::register_word& word, const linear::word_size size) {
        m_out << 'u';
        switch (size) {
            case linear::word_size::MICHAELCC_WORD_SIZE_BYTE: m_out << word.ubyte; break;
            case linear::word_size::MICHAELCC_WORD_SIZE_UINT16: m_out << word.uint16; break;
            case linear::word_size::MICHAELCC_WORD_SIZE_UINT32: m_out << word.uint32; break;
            case linear::word_size::MICHAELCC_WORD_SIZE_UINT64: m_out << word.uint64; break;
            default: throw std::runtime_error("Invalid word size");
        }
    }

    void print_indent() { 
        for (size_t i = 0; i < m_indent; i++) { m_out << "\t"; }
    }

public:
    linear_print_visitor(std::ostream& out, const linear::register_allocator& register_allocator, const platform_info& platform_info, int indent = 0) 
    : m_out(out), m_register_allocator(register_allocator), m_platform_info(platform_info), m_indent(indent) {}

    std::vector<size_t> print_basic_block(const linear::basic_block& node, std::optional<std::string> label = std::nullopt) {
        print_indent();
        if (label.has_value()) {
            m_out << label.value() << ":\n";
        }
        else {
            m_out << "basic_block(id=" << node.id() << "):\n";
        }
        m_indent++;
        for (const auto& instruction : node.instructions()) {
            (*this)(*instruction);
        }
        m_indent--;

        return subsequent_block_ids;
    }

protected:
    void dispatch(const linear::a_instruction& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = ";
        print_virtual_register(node.operand_a());
        m_out << " " << a_instruction_type_to_str(node.type()) << " ";
        print_virtual_register(node.operand_b());
        m_out << "\n";
    }

    void dispatch(const linear::a2_instruction& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = ";
        print_virtual_register(node.operand_a());
        m_out << " " << a_instruction_type_to_str(node.type()) << " " << node.constant() << " ";
        m_out << "\n";
    }

    void dispatch(const linear::u_instruction& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = " << u_instruction_type_to_str(node.type());
        print_virtual_register(node.operand());
        m_out << "\n";
    }

    void dispatch(const linear::init_register& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = ";
        print_register_word(node.value(), node.destination().reg_size);
        m_out << "\n";
    }

    void dispatch(const linear::load_memory& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = *(";
        print_virtual_register(node.source_address());
        m_out << " + " << node.offset() << ") ;(" << node.size_bytes() << " bytes)\n";
    }

    void dispatch(const linear::store_memory& node) override {
        print_indent();
        print_virtual_register(node.source_address(), true);
        m_out << " = *(";
        print_virtual_register(node.value());
        m_out << " + " << node.offset() << ") ;(" << node.size_bytes() << " bytes)\n";
    }

    void dispatch(const linear::alloca_instruction& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = alloca(size=" << node.size_bytes() << " bytes, alignment=" << node.alignment() << " bytes)\n";
    }

    void dispatch(const linear::valloca_instruction& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = valloca(size=";
        print_virtual_register(node.size());
        m_out << " bytes, alignment=" << node.alignment() << " bytes)\n";
    }

    void dispatch(const linear::branch& node) override {
        print_indent();
        m_out << "branch(block" << node.next_block_id() << ")\n";
        subsequent_block_ids.push_back(node.next_block_id());
    }

    void dispatch(const linear::branch_condition& node) override {
        print_indent();
        m_out << "branch_condition(condition=";
        print_virtual_register(node.condition(), true);
        m_out << ", true_block=" << node.if_true_block_id() << ", false_block=" << node.if_false_block_id() << ", is_loop=" << node.is_loop() << ")\n";
        subsequent_block_ids.push_back(node.if_true_block_id());
        subsequent_block_ids.push_back(node.if_false_block_id());
    }

    void dispatch(const linear::function_call& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = function_call(callee=";
        std::visit(overloaded{
            [this](const std::string& label) { m_out << label; },
            [this](const linear::virtual_register& reg) { print_virtual_register(reg, true); }
        }, node.callee());
        m_out << ", arguments=";
        for (const auto& arg : node.arguments()) {
            print_virtual_register(arg, true);
            m_out << ", ";
        }
        m_out << ")\n";
    }

    void dispatch(const linear::function_return& node) override {
        print_indent();
        m_out << "function_return\n";
    }

    void dispatch(const linear::phi_instruction& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = phi(";
        for (const auto& value : node.values()) {
            print_virtual_register(value.vreg, true);
            m_out << ", ";
        }
        m_out << ")\n";
    }

    void dispatch(const linear::load_effective_address& node) override {
        print_indent();
        print_virtual_register(node.destination(), true);
        m_out << " = load_effective_address(label=" << node.label() << ")\n";
    }
};

namespace michaelcc {
namespace logic {
    std::string to_tree_string(const translation_unit& unit) {
        std::stringstream ss;
        logical_print_visitor visitor(ss);
        unit.accept(visitor);
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
                
                printed_block_ids.insert(block_id);
                linear_print_visitor visitor(ss, unit.register_allocator, unit.platform_info, 0);
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