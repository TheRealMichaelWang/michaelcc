#include "parser.hpp"
#include <cctype>
#include <cstdint>
#include <sstream>

using namespace michaelcc;

const std::map<token_type, int> ast::arithmetic_operator::operator_precedence = {
    {MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR, 1},
    {MICHAELCC_TOKEN_INCREMENT_BY,        1},
    {MICHAELCC_TOKEN_DECREMENT_BY,        1},
    {MICHAELCC_TOKEN_PLUS,                2},
    {MICHAELCC_TOKEN_MINUS,               2},
    {MICHAELCC_TOKEN_ASTERISK,            3},
    {MICHAELCC_TOKEN_SLASH,               3},
    {MICHAELCC_TOKEN_CARET,               4},
    {MICHAELCC_TOKEN_AND,                 5},
    {MICHAELCC_TOKEN_OR,                  5},
    {MICHAELCC_TOKEN_DOUBLE_AND,          6},
    {MICHAELCC_TOKEN_DOUBLE_OR,           7},
    {MICHAELCC_TOKEN_MORE,                8},
    {MICHAELCC_TOKEN_LESS,                8},
    {MICHAELCC_TOKEN_MORE_EQUAL,          8},
    {MICHAELCC_TOKEN_LESS_EQUAL,          8},
    {MICHAELCC_TOKEN_EQUALS,              9},
    {MICHAELCC_TOKEN_QUESTION,			  1},
};

void michaelcc::parser::next_token() noexcept {
	for (;;) {
		if (end()) {
			return;
		}

		m_token_index++;

		if (current_token().type() == MICHAELCC_TOKEN_LINE_DIRECTIVE) {
			current_loc = current_token().location();
			continue;
		}
		else if (current_token().type() == MICHAELCC_TOKEN_NEWLINE) {
			current_loc.increment_line();
			continue;
		}
		else {
			current_loc.set_col(current_token().column());
			break;
		}
	}
}

void michaelcc::parser::match_token(token_type type) const
{
	if (current_token().type() != type) {
		std::stringstream ss;
		ss << "Expected " << token_to_str(type) << " but got " << token_to_str(current_token().type()) << " instead.";
		throw panic(ss.str());
	}
}

uint8_t michaelcc::parser::parse_storage_qualifiers()
{    
    // Parse qualifiers (const, volatile, etc.)
    uint8_t qualifiers = ast::NO_STORAGE_QUALIFIER;
    for (;;) {
        switch (current_token().type()) {
        case MICHAELCC_TOKEN_CONST: qualifiers |= ast::CONST_STORAGE_QUALIFIER; next_token(); continue;
        case MICHAELCC_TOKEN_VOLATILE: qualifiers |= ast::VOLATILE_STORAGE_QUALIFIER; next_token(); continue;
        case MICHAELCC_TOKEN_EXTERN: qualifiers |= ast::EXTERN_STORAGE_QUALIFIER; next_token(); continue;
        case MICHAELCC_TOKEN_STATIC: qualifiers |= ast::STATIC_STORAGE_QUALIFIER; next_token(); continue;
        case MICHAELCC_TOKEN_REGISTER: qualifiers |= ast::REGISTER_STORAGE_QUALIFIER; next_token(); continue;
        default:
            return qualifiers;
        }
    }
}

std::unique_ptr<ast::type> parser::parse_int_type() {
    source_location location = current_loc;
    uint8_t qualifiers = ast::NO_INT_QUALIFIER;
    ast::int_class int_cls = ast::INT_INT_CLASS;

    // Accept qualifiers and type specifiers in any order, as in C
    int long_count = 0;
    int tokens_saw = 0;
    for(;;) {
        switch (current_token().type()) {
        case MICHAELCC_TOKEN_SIGNED:
            qualifiers |= ast::SIGNED_INT_QUALIFIER;
            tokens_saw++;
            next_token();
            continue;
        case MICHAELCC_TOKEN_UNSIGNED:
            qualifiers |= ast::UNSIGNED_INT_QUALIFIER;
            tokens_saw++;
            next_token();
            continue;
        case MICHAELCC_TOKEN_SHORT:
            int_cls = ast::SHORT_INT_CLASS;
            tokens_saw++;
            next_token();
            continue;
        case MICHAELCC_TOKEN_LONG:
            long_count++;
            if (qualifiers & ast::LONG_INT_QUALIFIER) {
                int_cls = ast::LONG_INT_CLASS;
            }
            else {
                qualifiers |= ast::LONG_INT_QUALIFIER;
            }
            tokens_saw++;
            next_token();
            continue;
        case MICHAELCC_TOKEN_CHAR:
            int_cls = ast::CHAR_INT_CLASS;
            tokens_saw++;
            next_token();
            continue;
        case MICHAELCC_TOKEN_INT:
            tokens_saw++;
            next_token();
            continue;
        default:
            break;
        }
        break;
    }

    // Validate combinations according to C standard
    if (int_cls & ast::SHORT_INT_CLASS && (int_cls & ast::SHORT_INT_CLASS || long_count > 0)) {
        throw panic("Invalid combination: 'char' cannot be combined with 'short', 'long', or 'int'");
    }
    if (int_cls & ast::SHORT_INT_CLASS && long_count > 0) {
        throw panic("Invalid combination: 'short' and 'long' cannot be combined");
    }
    if (tokens_saw == 0) {
        throw panic("Unknown type specifier.");
    }

    return std::make_unique<ast::int_type>(qualifiers, int_cls, std::move(location));
}

std::unique_ptr<ast::type> michaelcc::parser::parse_type(const bool parse_pointer)
{
    source_location location = current_loc;
    std::unique_ptr<ast::type> base_type;

    // Parse float types
    if (current_token().type() == MICHAELCC_TOKEN_FLOAT ||
        current_token().type() == MICHAELCC_TOKEN_DOUBLE) {
        ast::float_class float_cls = (current_token().type() == MICHAELCC_TOKEN_FLOAT)
            ? ast::FLOAT_FLOAT_CLASS : ast::DOUBLE_FLOAT_CLASS;
        next_token();
        base_type = std::make_unique<ast::float_type>(float_cls, source_location(location));
    }
    else if (current_token().type() == MICHAELCC_TOKEN_STRUCT) {
        base_type = parse_struct_declaration();
    }
    else if (current_token().type() == MICHAELCC_TOKEN_UNION) {
        base_type = parse_union_declaration();
    }
    else if (current_token().type() == MICHAELCC_TOKEN_ENUM) {
        base_type = parse_enum_declaration();
    }
    else if (current_token().type() == MICHAELCC_TOKEN_VOID) {
        base_type = std::make_unique<ast::void_type>(source_location(location));
        next_token();
    }
    else if (current_token().type() == MICHAELCC_TOKEN_IDENTIFIER) {
        auto it = typedef_declarations.find(current_token().string());
        if (it == typedef_declarations.end()) {
            std::stringstream ss;
            ss << "Type definition " << current_token().string() << " does not exist.";
            throw panic(ss.str());
        }

        base_type = it->second->clone();
        next_token();
    }
    else {
        base_type = parse_int_type();
    }

    // Parse pointer and array modifiers
    while (parse_pointer) {
        if (current_token().type() == MICHAELCC_TOKEN_ASTERISK) {
            next_token();
            base_type = std::make_unique<ast::pointer_type>(std::move(base_type), source_location(location));
        }
        else {
            break;
        }
    }
    return base_type;
}

parser::declarator parser::parse_declarator() {
    source_location location = current_loc;
    std::unique_ptr<ast::type> current_type = parse_type(false);
    std::string identifier;

    // Handle pointer declarators (e.g., int *x)
    while (current_token().type() == MICHAELCC_TOKEN_ASTERISK) {
        next_token();
        current_type = std::make_unique<ast::pointer_type>(std::move(current_type), source_location(location));
    }

    // Handle identifier or nested declarator
    if (current_token().type() == MICHAELCC_TOKEN_IDENTIFIER) {
        identifier = current_token().string();
        next_token();
    }
    else if (current_token().type() == MICHAELCC_TOKEN_OPEN_PAREN) { //handle function pointer
        next_token();
        match_token(MICHAELCC_TOKEN_ASTERISK);
        next_token();
        if (current_token().type() == MICHAELCC_TOKEN_IDENTIFIER) {
            identifier = current_token().string();
            next_token();
        }
        match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
        next_token();

        match_token(MICHAELCC_TOKEN_OPEN_PAREN);

        next_token();
        std::vector<std::unique_ptr<ast::type>> parameter_types;
        while (current_token().type() != MICHAELCC_TOKEN_CLOSE_PAREN) {
            parameter_types.push_back(parse_type());
            if (current_token().type() == MICHAELCC_TOKEN_COMMA) {
                next_token();
            }
            else {
                break;
            }
        }
        match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
        next_token();
        return { std::make_unique<ast::function_pointer_type>(std::move(current_type), std::move(parameter_types), source_location(location)), identifier };
    }
    else {
        throw panic("Expected identifier or '(' in declarator");
    }

    // Handle array and function declarators
    while (true) {
        if (current_token().type() == MICHAELCC_TOKEN_OPEN_BRACKET) {
            // Array declarator (e.g., int x[10])
            next_token();
            std::optional<std::unique_ptr<ast::expression>> length;
            if (current_token().type() != MICHAELCC_TOKEN_CLOSE_BRACKET) {
                length = parse_expression();
            }
            match_token(MICHAELCC_TOKEN_CLOSE_BRACKET);
            next_token();
            current_type = std::make_unique<ast::array_type>(std::move(current_type), std::move(length), source_location(location));
        }
        else {
            break;
        }
    }

    return { std::move(current_type), std::move(identifier) };
}


std::unique_ptr<ast::set_destination> michaelcc::parser::parse_set_accessors(std::unique_ptr<ast::set_destination>&& initial_value)
{
	std::unique_ptr<ast::set_destination> value = std::move(initial_value);

	for (;;) {
		source_location location = current_loc;
		switch (current_token().type())
		{
		case MICHAELCC_TOKEN_OPEN_BRACKET:
			next_token();
			value = std::make_unique<ast::get_index>(std::move(value), parse_expression(), std::move(location));
			match_token(MICHAELCC_TOKEN_CLOSE_BRACKET);
			next_token();
			break;
		case MICHAELCC_TOKEN_PERIOD:
			next_token();
			match_token(MICHAELCC_TOKEN_IDENTIFIER);
			value = std::make_unique<ast::get_property>(std::move(value), std::string(current_token().string()), false, std::move(location));
			next_token();
			break;
		case MICHAELCC_TOKEN_DEREFERENCE_GET:
			next_token();
			match_token(MICHAELCC_TOKEN_IDENTIFIER);
			value = std::make_unique<ast::get_property>(std::move(value), std::string(current_token().string()), true, std::move(location));
			next_token();
			break;
		default:
            return value;
		}
	}
}

std::unique_ptr<ast::set_destination> michaelcc::parser::parse_set_destination()
{
	source_location location = current_loc;
	std::unique_ptr<ast::set_destination> value;
	switch (current_token().type())
	{
	case MICHAELCC_TOKEN_IDENTIFIER:
		value = std::make_unique<ast::variable_reference>(scan_token().string(), std::move(location));
		break;
	case MICHAELCC_TOKEN_ASTERISK:
		return std::make_unique<ast::dereference_operator>(parse_value(), std::move(location));
	default: {
		std::stringstream ss;
		ss << "Unexpected token " << token_to_str(current_token().type()) << '.';
		throw panic(ss.str());
	}
	}

	return parse_set_accessors(std::move(value));
}

std::unique_ptr<ast::expression> michaelcc::parser::parse_value()
{
	source_location location = current_loc;
    std::unique_ptr<ast::expression> value;

	switch (current_token().type())
	{
	case MICHAELCC_TOKEN_FLOAT32_LITERAL:
		value = std::make_unique<ast::float_literal>(scan_token().float32(), source_location(location));
        break;
	case MICHAELCC_TOKEN_FLOAT64_LITERAL:
		value = std::make_unique<ast::double_literal>(scan_token().float64(), source_location(location));
        break;
	case MICHAELCC_TOKEN_INTEGER_LITERAL:
		value = std::make_unique<ast::int_literal>(ast::NO_INT_QUALIFIER, ast::INT_INT_CLASS, scan_token().integer(), source_location(location));
        break;
	case MICHAELCC_TOKEN_CHAR_LITERAL:
		value = std::make_unique<ast::int_literal>(ast::NO_INT_QUALIFIER, ast::CHAR_INT_CLASS, scan_token().integer(), source_location(location));
        break;
	case MICHAELCC_TOKEN_OPEN_PAREN: {
		next_token();
		std::unique_ptr<ast::expression> expression = parse_expression();
		match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
		next_token();
		return expression;
	}
    default: {
        std::unique_ptr<ast::set_destination> destination = parse_set_destination();
        if (current_token().type() == MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR) {
            next_token();
            return std::make_unique<ast::set_operator>(std::move(destination), parse_expression(), std::move(location));
        }
        
        value = std::move(destination);
        break;
    }
	}

    for (;;) {
        switch (current_token().type())
        {
        case MICHAELCC_TOKEN_OPEN_PAREN: {
            next_token();
            std::vector<std::unique_ptr<ast::expression>> arguments;
            while (current_token().type() != MICHAELCC_TOKEN_CLOSE_PAREN && !end()) {
                arguments.push_back(parse_expression());
                if (current_token().type() == MICHAELCC_TOKEN_COMMA) {
                    next_token();
                }
                else {
                    break;
                }
            }
            match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
            next_token();
            
            value = std::make_unique<ast::function_call>(std::move(value), std::move(arguments), source_location(location));
            break;
        }
        default:
            return value;
        }
    }
}

std::unique_ptr<ast::expression> michaelcc::parser::parse_expression(int min_precedence)
{ 
	// Parse the leftmost value (could be a literal, variable, or parenthesized expression)
	std::unique_ptr<ast::expression> left = parse_value();

    while (current_token().is_operator()) {
        auto it = ast::arithmetic_operator::operator_precedence.find(current_token().type());
        if (it == ast::arithmetic_operator::operator_precedence.end()) break;

        int precedence = it->second;
        if (precedence < min_precedence) break;

        token_type op = current_token().type();
        source_location op_loc = current_loc;
        next_token();

        // For right-associative operators (like assignment), use precedence, not precedence+1
        int next_min_prec = precedence + 1;
        // If op is right-associative, use 'precedence' instead
        // (For most C/C++ binary ops, left-associative, so +1 is correct.)

        if (op == MICHAELCC_TOKEN_QUESTION) {

            // Parse the 'true' branch (middle expression)
            auto true_expr = parse_expression(precedence); // precedence 0, as if parenthesized

            // Expect and consume ':'
            match_token(MICHAELCC_TOKEN_COLON);
            next_token();

            // Parse the 'false' branch (right expression)
            auto false_expr = parse_expression(precedence); // precedence 0, as if parenthesized

            left = std::make_unique<ast::conditional_expression>(
                std::move(left), std::move(true_expr), std::move(false_expr), std::move(op_loc));
            continue;
        }
        else {
            std::unique_ptr<ast::expression> right = parse_expression(next_min_prec);
            left = std::make_unique<ast::arithmetic_operator>(op, std::move(left), std::move(right), std::move(op_loc));
        }
    }

	return left;
}

std::unique_ptr<ast::statement> parser::parse_statement() {
    source_location location = current_loc;

    switch (current_token().type()) {
    case MICHAELCC_TOKEN_IF: {
        next_token();
        match_token(MICHAELCC_TOKEN_OPEN_PAREN);
        next_token();
        auto condition = parse_expression();
        match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
        next_token();
        auto true_block = parse_block();
        if (current_token().type() == MICHAELCC_TOKEN_ELSE) {
            next_token();
            auto false_block = parse_block();
            return std::make_unique<ast::if_else_block>(
                std::move(condition),
                std::move(true_block),
                std::move(false_block),
                std::move(location)
            );
        }
        else {
            return std::make_unique<ast::if_block>(
                std::move(condition),
                std::move(true_block),
                std::move(location)
            );
        }
    }
    case MICHAELCC_TOKEN_WHILE: {
        next_token();
        match_token(MICHAELCC_TOKEN_OPEN_PAREN);
        next_token();
        auto condition = parse_expression();
        match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
        next_token();
        auto body = parse_block();
        return std::make_unique<ast::while_block>(
            std::move(condition),
            std::move(body),
            std::move(location)
        );
    }
    case MICHAELCC_TOKEN_DO: {
        next_token();
        auto body = parse_block();
        match_token(MICHAELCC_TOKEN_WHILE);
        next_token();
        match_token(MICHAELCC_TOKEN_OPEN_PAREN);
        next_token();
        auto condition = parse_expression();
        match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
        next_token();
        match_token(MICHAELCC_TOKEN_SEMICOLON);
        next_token();
        return std::make_unique<ast::do_block>(
            std::move(condition),
            std::move(body),
            std::move(location)
        );
    }
    case MICHAELCC_TOKEN_FOR: {
        next_token();
        match_token(MICHAELCC_TOKEN_OPEN_PAREN);
        next_token();
        auto init = parse_statement();
        auto cond = parse_expression();
        match_token(MICHAELCC_TOKEN_SEMICOLON);
        next_token();
        auto inc = parse_statement();
        match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
        next_token();
        auto body = parse_block();
        return std::make_unique<ast::for_loop>(
            std::move(init),
            std::move(cond),
            std::move(inc),
            std::move(body),
            std::move(location)
        );
    }
    case MICHAELCC_TOKEN_RETURN: {
        next_token();
        std::unique_ptr<ast::expression> value;
        if (current_token().type() != MICHAELCC_TOKEN_SEMICOLON) {
            value = parse_expression();
        }
        match_token(MICHAELCC_TOKEN_SEMICOLON);
        next_token();
        return std::make_unique<ast::return_statement>(std::move(value), std::move(location));
    }
    case MICHAELCC_TOKEN_BREAK: {
        next_token();
        int break_depth = 1;
        if (current_token().type() == MICHAELCC_TOKEN_INTEGER_LITERAL) {
            break_depth = current_token().integer();
            if (break_depth < 1) {
                throw panic("Break statement depth cannot be less than 1.");
            }
            next_token();
        }
        match_token(MICHAELCC_TOKEN_SEMICOLON);
        next_token();
        return std::make_unique<ast::break_statement>(break_depth, std::move(location)); // loop depth can be set as needed
    }
    case MICHAELCC_TOKEN_CONTINUE: {
        next_token();
        int continue_depth = 1;
        if (current_token().type() == MICHAELCC_TOKEN_INTEGER_LITERAL) {
            continue_depth = current_token().integer();
            if (continue_depth < 1) {
                throw panic("Continue statement depth cannot be less than 1.");
            }
            next_token();
        }
        match_token(MICHAELCC_TOKEN_SEMICOLON);
        next_token();
        return std::make_unique<ast::continue_statement>(0, std::move(location)); // loop depth can be set as needed
    }
    case MICHAELCC_TOKEN_OPEN_BRACE:
        return std::make_unique<ast::context_block>(parse_block());
    case MICHAELCC_TOKEN_CONST:
    case MICHAELCC_TOKEN_VOLATILE:
    case MICHAELCC_TOKEN_EXTERN:
    case MICHAELCC_TOKEN_STATIC:
    case MICHAELCC_TOKEN_REGISTER:
    case MICHAELCC_TOKEN_SIGNED:
    case MICHAELCC_TOKEN_UNSIGNED:
    case MICHAELCC_TOKEN_SHORT:
    case MICHAELCC_TOKEN_LONG:
    case MICHAELCC_TOKEN_CHAR:
    case MICHAELCC_TOKEN_INT:
    case MICHAELCC_TOKEN_FLOAT:
    case MICHAELCC_TOKEN_DOUBLE:
        return std::make_unique<ast::variable_declaration>(parse_variable_declaration());
    default: {
        // Expression or declaration statement
        std::unique_ptr<ast::expression> expr = parse_expression();
        match_token(MICHAELCC_TOKEN_SEMICOLON);
        next_token();

        ast::statement* statement = dynamic_cast<ast::statement*>(expr.get());
        if (statement == nullptr) {
            throw panic("Expected statement but got expression instead.");
        }

        expr.release();
        // Wrap as an expression statement or declaration as appropriate
        return std::unique_ptr<ast::statement>(statement); // If you have a separate expression_statement AST node, wrap it here
    }
    }
}

ast::context_block parser::parse_block() {
    source_location location = current_loc;
    match_token(MICHAELCC_TOKEN_OPEN_BRACE);
    next_token();

    std::vector<std::unique_ptr<ast::statement>> statements;
    while (current_token().type() != MICHAELCC_TOKEN_CLOSE_BRACE && !end()) {
        statements.push_back(parse_statement());
    }

    match_token(MICHAELCC_TOKEN_CLOSE_BRACE);
    next_token();

    return ast::context_block(std::move(statements), std::move(location));
}

ast::variable_declaration parser::parse_variable_declaration() {
    source_location location = current_loc;
    uint8_t qualifiers = parse_storage_qualifiers();
    declarator decl = parse_declarator();
    std::optional<std::unique_ptr<ast::expression>> initializer;
    if (current_token().type() == MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR) {
        next_token();
        initializer = parse_expression();
    }
    match_token(MICHAELCC_TOKEN_SEMICOLON);
    next_token();
    return ast::variable_declaration(
        qualifiers, std::move(decl.type), decl.identifier, std::move(location), std::move(initializer)
    );
}

std::unique_ptr<ast::struct_declaration> parser::parse_struct_declaration() {
    source_location location = current_loc;
    match_token(MICHAELCC_TOKEN_STRUCT);
    next_token();

    std::optional<std::string> struct_name;
    if (current_token().type() == MICHAELCC_TOKEN_IDENTIFIER) {
        struct_name = current_token().string();
        next_token();
    }

    if (current_token().type() != MICHAELCC_TOKEN_OPEN_BRACE) {
        return std::make_unique<ast::struct_declaration>(std::move(struct_name), std::vector<ast::variable_declaration>(), std::move(location));
    }
    next_token();

    std::vector<ast::variable_declaration> members;
    while (current_token().type() != MICHAELCC_TOKEN_CLOSE_BRACE && !end()) {
        members.emplace_back(parse_variable_declaration());
    }

    match_token(MICHAELCC_TOKEN_CLOSE_BRACE);
    next_token();

    return std::make_unique<ast::struct_declaration>(std::move(struct_name), std::move(members), std::move(location));
}

std::unique_ptr<ast::union_declaration> parser::parse_union_declaration() {
    source_location location = current_loc;
    match_token(MICHAELCC_TOKEN_UNION);
    next_token();

    std::optional<std::string> union_name;
    if (current_token().type() == MICHAELCC_TOKEN_IDENTIFIER) {
        union_name = current_token().string();
        next_token();
    }

    if (current_token().type() != MICHAELCC_TOKEN_OPEN_BRACE) {
        return std::make_unique<ast::union_declaration>(std::move(union_name), std::vector<ast::union_declaration::member>(), std::move(location));
    }
    next_token();

    std::vector<ast::union_declaration::member> members;
    while (current_token().type() != MICHAELCC_TOKEN_CLOSE_BRACE && !end()) {
        auto type = parse_type();
        if (current_token().type() != MICHAELCC_TOKEN_IDENTIFIER) {
            throw panic("Expected identifier in union member.");
        }
        std::string member_name = current_token().string();
        next_token();
        match_token(MICHAELCC_TOKEN_SEMICOLON);
        next_token();
        members.emplace_back(std::move(type), std::move(member_name));
    }

    match_token(MICHAELCC_TOKEN_CLOSE_BRACE);
    next_token();

    return std::make_unique<ast::union_declaration>(std::move(union_name), std::move(members), std::move(location));
}

std::unique_ptr<ast::enum_declaration> parser::parse_enum_declaration() {
    source_location location = current_loc;
    match_token(MICHAELCC_TOKEN_ENUM);
    next_token();

    std::optional<std::string> enum_name;
    if (current_token().type() == MICHAELCC_TOKEN_IDENTIFIER) {
        enum_name = current_token().string();
        next_token();
    }

    if (current_token().type() != MICHAELCC_TOKEN_OPEN_BRACE) {
        return std::make_unique<ast::enum_declaration>(std::move(enum_name), std::vector<ast::enum_declaration::enumerator>(), std::move(location));
    }
    next_token();

    std::vector<ast::enum_declaration::enumerator> enumerators;
    while (current_token().type() != MICHAELCC_TOKEN_CLOSE_BRACE && !end()) {
        if (current_token().type() != MICHAELCC_TOKEN_IDENTIFIER) {
            throw panic("Expected identifier in enum.");
        }
        std::string name = current_token().string();
        next_token();

        std::optional<int64_t> value;
        if (current_token().type() == MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR) {
            next_token();
            auto ast_value = parse_expression();
            ast::int_literal* int_value = dynamic_cast<ast::int_literal*>(ast_value.get());
            if (int_value == nullptr) {
                throw panic("You must set enumerator to specific integer value.");
            }
            value = int_value->signed_value();
        }

        enumerators.emplace_back(ast::enum_declaration::enumerator(std::move(name)));

        if (current_token().type() == MICHAELCC_TOKEN_COMMA) {
            next_token();
        }
        else {
            break;
        }
    }

    match_token(MICHAELCC_TOKEN_CLOSE_BRACE);
    next_token();

    return std::make_unique<ast::enum_declaration>(std::move(enum_name), std::move(enumerators), std::move(location));
}

std::unique_ptr<ast::typedef_declaration> michaelcc::parser::parse_typedef_declaration() {
    source_location location = current_loc;
    match_token(MICHAELCC_TOKEN_TYPEDEF);
    next_token();
    
    declarator decl = parse_declarator();
    auto it = typedef_declarations.find(decl.identifier);
    if (it != typedef_declarations.end()) {
        std::stringstream ss;
        ss << "typedef ";
        it->second->build_declarator(ss, decl.identifier);
        ss << " already exists.";
        throw panic(ss.str());
    }
    typedef_declarations.insert({ decl.identifier, decl.type.get() });

    return std::make_unique<ast::typedef_declaration>(std::move(decl.type), std::move(decl.identifier), std::move(location));
}

std::vector<ast::function_parameter> parser::parse_parameter_list() {
    std::vector<ast::function_parameter> params;
    match_token(MICHAELCC_TOKEN_OPEN_PAREN);
    next_token();
    while (current_token().type() != MICHAELCC_TOKEN_CLOSE_PAREN) {
        uint8_t qualifiers = parse_storage_qualifiers();
        
        declarator decl = parse_declarator();

        params.emplace_back(std::move(decl.type), std::move(decl.identifier), qualifiers);

        if (current_token().type() != MICHAELCC_TOKEN_CLOSE_PAREN) {
            match_token(MICHAELCC_TOKEN_COMMA);
            next_token();
        }
    }
    match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
    next_token();
    return params;
}

std::unique_ptr<ast::function_prototype> parser::parse_function_prototype() {
    source_location location = current_loc;
    
    auto return_type = parse_type();
    std::string func_name = current_token().string();
    next_token();
    auto params = parse_parameter_list();
    match_token(MICHAELCC_TOKEN_SEMICOLON);
    next_token();

    return std::make_unique<ast::function_prototype>(
        std::move(return_type), std::move(func_name), std::move(params), std::move(location)
    );
}

std::unique_ptr<ast::function_declaration> parser::parse_function_declaration() {
    source_location location = current_loc;

    auto return_type = parse_type();
    std::string func_name = current_token().string();
    next_token();
    auto params = parse_parameter_list();
    auto body = parse_block();
    
    return std::make_unique<ast::function_declaration>(
        std::move(return_type), std::move(func_name), std::move(params), std::move(body), std::move(location)
    );
}

std::vector<std::unique_ptr<ast::top_level_element>> michaelcc::parser::parse_all()
{
    std::vector<std::unique_ptr<ast::top_level_element>> top_level_elements;

    while (!end())
    {
        switch (current_token().type())
        {
        case MICHAELCC_TOKEN_STRUCT:
            top_level_elements.emplace_back(parse_struct_declaration());
            break;
        case MICHAELCC_TOKEN_UNION:
            top_level_elements.emplace_back(parse_union_declaration());
            break;
        case MICHAELCC_TOKEN_ENUM:
            top_level_elements.emplace_back(parse_enum_declaration());
            break;
        case MICHAELCC_TOKEN_TYPEDEF:
            top_level_elements.emplace_back(parse_typedef_declaration());
            break;
        case MICHAELCC_TOKEN_CONST:
        case MICHAELCC_TOKEN_VOLATILE:
        case MICHAELCC_TOKEN_EXTERN:
        case MICHAELCC_TOKEN_STATIC:
        case MICHAELCC_TOKEN_REGISTER:
        case MICHAELCC_TOKEN_SIGNED:
        case MICHAELCC_TOKEN_UNSIGNED:
        case MICHAELCC_TOKEN_SHORT:
        case MICHAELCC_TOKEN_LONG:
        case MICHAELCC_TOKEN_CHAR:
        case MICHAELCC_TOKEN_INT:
        case MICHAELCC_TOKEN_FLOAT:
        case MICHAELCC_TOKEN_DOUBLE:
        case MICHAELCC_TOKEN_IDENTIFIER:
        case MICHAELCC_TOKEN_VOID: {
            // Try to parse as function prototype or declaration
            size_t backup = m_token_index;
            try {
                auto return_type = parse_type();
                if (current_token().type() == MICHAELCC_TOKEN_IDENTIFIER) {
                    std::string func_name = current_token().string();
                    next_token();
                    if (current_token().type() == MICHAELCC_TOKEN_OPEN_PAREN) {
                        // Look ahead to distinguish prototype vs. declaration
                        size_t param_backup = m_token_index;
                        parse_parameter_list();
                        if (current_token().type() == MICHAELCC_TOKEN_SEMICOLON) {
                            m_token_index = backup;
                            top_level_elements.emplace_back(parse_function_prototype());
                            continue;
                        }
                        else if (current_token().type() == MICHAELCC_TOKEN_OPEN_BRACE) {
                            m_token_index = backup;
                            top_level_elements.emplace_back(parse_function_declaration());
                            continue;
                        }
                    }
                }
            }
            catch (...) {
                m_token_index = backup; // restore if not a function
            }
            // If not a function, treat as variable declaration
            m_token_index = backup;
            top_level_elements.emplace_back(std::make_unique<ast::variable_declaration>(parse_variable_declaration()));
            continue;
        }
        default:
            break;
        }

        match_token(MICHAELCC_TOKEN_SEMICOLON);
        next_token();
    }

    return top_level_elements;
}