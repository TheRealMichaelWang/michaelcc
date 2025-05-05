#pragma once

#include "tokens.hpp"
#include "ast.hpp"
#include "errors.hpp"

namespace michaelcc {
	class parser {
	private:
		std::vector<token> m_tokens;
		size_t m_token_index;
		source_location current_loc;

		const bool end() const noexcept {
			return m_tokens.size() == m_token_index;
		}

		const token& current_token() const {
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

		std::unique_ptr<ast::lvalue> parse_accessors(std::unique_ptr<ast::lvalue>&& initial_value);

		std::unique_ptr<ast::rvalue> parse_individual_rvalue();
		std::unique_ptr<ast::rvalue> parse_rvalue();

		std::unique_ptr<ast::lvalue> parse_value();
		std::unique_ptr<ast::lvalue> parse_expression(int min_precedence);

		std::unique_ptr<ast::statement> parse_statement();
		std::unique_ptr<ast::context_block> parse_block();

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