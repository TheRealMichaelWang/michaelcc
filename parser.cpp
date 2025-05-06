#include "parser.hpp"
#include <cctype>
#include <cstdint>
#include <sstream>

using namespace michaelcc;

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

std::unique_ptr<ast::set_destination> michaelcc::parser::parse_accessors(std::unique_ptr<ast::set_destination>&& initial_value)
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
			break;
		}
	}

	return value;
}

std::unique_ptr<ast::set_destination> michaelcc::parser::parse_lvalue()
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

	return parse_accessors(std::move(value));
}

std::unique_ptr<ast::expression> michaelcc::parser::parse_value()
{
	source_location location = current_loc;
	switch (current_token().type())
	{
	case MICHAELCC_TOKEN_FLOAT32_LITERAL:
		return std::make_unique<ast::float_literal>(scan_token().float32(), std::move(location));
	case MICHAELCC_TOKEN_FLOAT64_LITERAL:
		return std::make_unique<ast::double_literal>(scan_token().float64(), std::move(location));
	case MICHAELCC_TOKEN_INTEGER_LITERAL:
		return std::make_unique<ast::int_literal>(ast::NO_INT_QUALIFIER, scan_token().integer(), std::move(location));
	case MICHAELCC_TOKEN_CHAR_LITERAL:
		return std::make_unique<ast::int_literal>(ast::CHAR_LITERAL_QUALIFIER, scan_token().integer(), std::move(location));
	case MICHAELCC_TOKEN_OPEN_PAREN: {
		next_token();
		std::unique_ptr<ast::expression> expression = parse_expression();
		match_token(MICHAELCC_TOKEN_CLOSE_PAREN);
		next_token();
		return expression;
	}
    default: {
        std::unique_ptr<ast::set_destination> destination = parse_lvalue();
        if (current_token().type() == MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR) {
            next_token();
            return std::make_unique<ast::set_operator>(std::move(destination), parse_expression(), std::move(location));
        }
        
        return destination;
    }
	}
}

std::unique_ptr<ast::expression> michaelcc::parser::parse_expression(int min_precedence)
{ 
	// Parse the leftmost value (could be a literal, variable, or parenthesized expression)
	std::unique_ptr<ast::expression> left = parse_value();

	while (current_token().is_operator()) {
		auto it = operator_precedence.find(current_token().type());
		if (it == operator_precedence.end()) break;

		int precedence = it->second;
		if (precedence < min_precedence) break;

		token_type op = current_token().type();
		source_location op_loc = current_loc;
		next_token();

		// For right-associative operators (like assignment), use precedence, not precedence+1
		int next_min_prec = precedence + 1;
		// If op is right-associative, use 'precedence' instead
		// (For most C/C++ binary ops, left-associative, so +1 is correct.)

		std::unique_ptr<ast::expression> right = parse_expression(next_min_prec);

		left = std::make_unique<ast::arithmetic_operator>(op, std::move(left), std::move(right), std::move(op_loc));
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
        if (current_token().type() != MICHAELCC_TOKEN_SEMICOLON)
            value = parse_expression();
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
        return std::make_unique<ast::statement>(*statement); // If you have a separate expression_statement AST node, wrap it here
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
