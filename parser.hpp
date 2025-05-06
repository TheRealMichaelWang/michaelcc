#pragma once

#include <array>
#include "tokens.hpp"
#include "ast.hpp"
#include "errors.hpp"

namespace michaelcc {
	class parser {
	private:
		const std::map<token_type, int> operator_precedence = {
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
			{MICHAELCC_TOKEN_EQUALS,              9}
		};

		std::vector<token> m_tokens;
		size_t m_token_index;
		source_location current_loc;

		const bool end() const noexcept {
			return m_tokens.size() == m_token_index;
		}

		const token current_token() const {
			if (end()) {
				return token(MICHAELCC_TOKEN_END, current_loc.col());
			}
			
			return m_tokens.at(m_token_index);
		}

		void next_token() noexcept;
		void match_token(token_type type) const;

		const token scan_token() {
			token tok = current_token();
			next_token();
			return tok;
		}

		std::unique_ptr<ast::set_destination> parse_accessors(std::unique_ptr<ast::set_destination>&& initial_value);

		std::unique_ptr<ast::set_destination> parse_lvalue();

		std::unique_ptr<ast::expression> parse_value();
		std::unique_ptr<ast::expression> parse_expression(int min_precedence=0);

		std::unique_ptr<ast::statement> parse_statement();
		ast::context_block parse_block();

		const compilation_error panic(const std::string msg) const noexcept {
			return compilation_error(msg, current_loc);
		}
	public:
		parser(std::vector<token>&& tokens) :
			m_tokens(std::move(tokens)), 
			m_token_index(0),
			current_loc(0, 0, "invalid_file") { }
	};
}