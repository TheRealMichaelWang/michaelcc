#include "tokens.hpp"
#include "ast.hpp"
#include "typing.hpp"
#include "logical.hpp"
#include <cctype>
#include <iomanip>

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
            print(*node.set_value());
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
        if (node.feilds().empty()) {
            return;
        }
        m_out << " {\n";
        m_indent++;
        for (const auto& field : node.feilds()) {
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
    std::string to_c_string(const ast_element* elem, int indent) {
        std::stringstream ss;
        ast_print_visitor visitor(ss, indent);
        visitor.print(*elem);
        
        // Add semicolon for top-level declarations that need them
        if (dynamic_cast<const variable_declaration*>(elem) ||
            dynamic_cast<const typedef_declaration*>(elem)) {
            ss << ";";
        }
        
        return ss.str();
    }
}
}

// Simple tree printer for logical IR using stack-based indent tracking
class logical_print_visitor : public logical_ir::visitor {
private:
    std::ostream& m_out;
    int m_indent = 0;
    std::vector<int> m_child_stack;

    void print_indent() { m_out << std::string(m_indent * 2, ' '); }

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
    void visit(const logical_ir::translation_unit& node) override {
        before_print();
        print_indent();
        m_out << "translation_unit\n";
        // Count how many symbols will be visited (see translation_unit::accept)
        int child_count = 0;
        for (const auto& sym : node.global_symbols()) {
            if (dynamic_cast<logical_ir::variable*>(sym.get()) ||
                dynamic_cast<logical_ir::function_definition*>(sym.get())) {
                child_count++;
            }
        }
        after_print(child_count);
    }

    // variable: 0 children
    void visit(const logical_ir::variable& node) override {
        before_print();
        print_indent();
        m_out << "variable: " << node.name() << (node.is_global() ? " (global)" : " (local)") << "\n";
        after_print(0);
    }

    // function_definition: params.size() + (body ? 1 : 0)
    void visit(const logical_ir::function_definition& node) override {
        before_print();
        print_indent();
        m_out << "function_definition: " << node.name() << "\n";
        int child_count = static_cast<int>(node.parameters().size()) + (node.body() ? 1 : 0);
        after_print(child_count);
    }

    // control_block: statements.size()
    void visit(const logical_ir::control_block& node) override {
        before_print();
        print_indent();
        m_out << "control_block\n";
        after_print(static_cast<int>(node.statements().size()));
    }

    // integer_constant: 0 children
    void visit(const logical_ir::integer_constant& node) override {
        before_print();
        print_indent();
        m_out << "integer_constant: " << node.value() << "\n";
        after_print(0);
    }

    // floating_constant: 0 children
    void visit(const logical_ir::floating_constant& node) override {
        before_print();
        print_indent();
        m_out << "floating_constant: " << node.value() << "\n";
        after_print(0);
    }

    // string_constant: 0 children
    void visit(const logical_ir::string_constant& node) override {
        before_print();
        print_indent();
        m_out << "string_constant: [index " << node.index() << "]\n";
        after_print(0);
    }

    // variable_reference: 0 children (doesn't visit the variable)
    void visit(const logical_ir::variable_reference& node) override {
        before_print();
        print_indent();
        m_out << "variable_reference: " << node.get_variable()->name() << "\n";
        after_print(0);
    }

    // binary_operation: 2 children (left, right)
    void visit(const logical_ir::binary_operation& node) override {
        before_print();
        print_indent();
        m_out << "binary_operation: " << token_to_str(node.get_operator()) << "\n";
        after_print(2);
    }

    // unary_operation: 1 child (operand)
    void visit(const logical_ir::unary_operation& node) override {
        before_print();
        print_indent();
        m_out << "unary_operation: " << token_to_str(node.get_operator()) << "\n";
        after_print(1);
    }

    // type_cast: 1 child (operand)
    void visit(const logical_ir::type_cast& node) override {
        before_print();
        print_indent();
        m_out << "type_cast\n";
        after_print(1);
    }

    // address_of: 1 child (operand)
    void visit(const logical_ir::address_of& node) override {
        before_print();
        print_indent();
        m_out << "address_of\n";
        after_print(1);
    }

    // dereference: 1 child (operand)
    void visit(const logical_ir::dereference& node) override {
        before_print();
        print_indent();
        m_out << "dereference\n";
        after_print(1);
    }

    // member_access: 1 child (base)
    void visit(const logical_ir::member_access& node) override {
        before_print();
        print_indent();
        m_out << "member_access: [field " << node.field_index() << "]\n";
        after_print(1);
    }

    // array_index: 2 children (base, index)
    void visit(const logical_ir::array_index& node) override {
        before_print();
        print_indent();
        m_out << "array_index\n";
        after_print(2);
    }

    // function_call: 1 + arguments.size() (callee + args)
    void visit(const logical_ir::function_call& node) override {
        before_print();
        print_indent();
        m_out << "function_call\n";
        after_print(1 + static_cast<int>(node.arguments().size()));
    }

    // conditional_expression: 3 children (condition, then, else)
    void visit(const logical_ir::conditional_expression& node) override {
        before_print();
        print_indent();
        m_out << "conditional_expression\n";
        after_print(3);
    }

    // expression_statement: 1 child (expression)
    void visit(const logical_ir::expression_statement& node) override {
        before_print();
        print_indent();
        m_out << "expression_statement\n";
        after_print(1);
    }

    // assignment_statement: 2 children (destination, value)
    void visit(const logical_ir::assignment_statement& node) override {
        before_print();
        print_indent();
        m_out << "assignment_statement\n";
        after_print(2);
    }

    // local_declaration: 1 + (initializer ? 1 : 0)
    void visit(const logical_ir::local_declaration& node) override {
        before_print();
        print_indent();
        m_out << "local_declaration\n";
        after_print(1 + (node.initializer() ? 1 : 0));
    }

    // return_statement: (value ? 1 : 0)
    void visit(const logical_ir::return_statement& node) override {
        before_print();
        print_indent();
        m_out << "return_statement\n";
        after_print(node.value() ? 1 : 0);
    }

    // if_statement: 2 + (else_body ? 1 : 0)
    void visit(const logical_ir::if_statement& node) override {
        before_print();
        print_indent();
        m_out << "if_statement\n";
        after_print(2 + (node.else_body() ? 1 : 0));
    }

    // loop_statement: 2 children (condition, body)
    void visit(const logical_ir::loop_statement& node) override {
        before_print();
        print_indent();
        m_out << "loop_statement" << (node.check_condition_first() ? " (while)" : " (do-while)") << "\n";
        after_print(2);
    }

    // break_statement: 0 children
    void visit(const logical_ir::break_statement& node) override {
        before_print();
        print_indent();
        m_out << "break_statement\n";
        after_print(0);
    }

    // continue_statement: 0 children
    void visit(const logical_ir::continue_statement& node) override {
        before_print();
        print_indent();
        m_out << "continue_statement\n";
        after_print(0);
    }
};

namespace michaelcc {
namespace logical_ir {
    std::string to_tree_string(const translation_unit& unit) {
        std::stringstream ss;
        logical_print_visitor visitor(ss);
        unit.accept(visitor);
        return ss.str();
    }
}
}
