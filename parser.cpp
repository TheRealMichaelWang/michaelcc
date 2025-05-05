#include "parser.hpp"
#include <cctype>
#include <cstdint>

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
		
	}
}

std::unique_ptr<ast::lvalue> michaelcc::parser::parse_accessors(std::unique_ptr<ast::lvalue>&& initial_value)
{
	std::unique_ptr<ast::lvalue> value = std::move(initial_value);

	for (;;) {
		switch (current_token().type())
		{
		case MICHAELCC_TOKEN_OPEN_BRACKET:
			next_token();
			value = std::make_unique<ast::get_index>(value, parse_rvalue());
			
		default:
			break;
		}
	}
}

std::unique_ptr<ast::rvalue> michaelcc::parser::parse_individual_rvalue()
{
	switch (current_token().type())
	{
	case MICHAELCC_TOKEN_IDENTIFIER:
		return std::make_unique<ast::variable_reference>(scan_token().string());
	case MICHAELCC_TOKEN_ASTERISK:
		return std::make_unique<ast::dereference_operator>(parse_value());
	default:
		throw std::exception();
	}
}

std::unique_ptr<ast::rvalue> michaelcc::parser::parse_rvalue()
{
	std::unique_ptr<ast::rvalue> rvalue = parse_individual_rvalue();

	if (current_token().type() == MICHAELCC_TOKEN_OPEN_BRACKET)
	{
		ast::lvalue* lvalue = dynamic_cast<ast::lvalue*>(rvalue.get());
		if (lvalue == NULL) {
			throw panic("Expected lvalue but got rvalue instead.");
		}

	}
}

std::unique_ptr<ast::lvalue> michaelcc::parser::parse_value()
{
	switch (current_token().type())
	{
	case MICHAELCC_TOKEN_FLOAT32_LITERAL:
		return std::make_unique<ast::float_literal>(scan_token().float32());
	case MICHAELCC_TOKEN_FLOAT64_LITERAL:
		return std::make_unique<ast::double_literal>(scan_token().float64());
	case MICHAELCC_TOKEN_INTEGER_LITERAL:
		return std::make_unique<ast::int_literal>(ast::NO_INT_QUALIFIER, ast::INT_INT_CLASS, scan_token().integer());
	case MICHAELCC_TOKEN_CHAR_LITERAL:
		return std::make_unique<ast::int_literal>(ast::NO_INT_QUALIFIER, ast::CHAR_INT_CLASS, scan_token().integer());
	default:
		break;
	}
}
