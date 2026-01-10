#ifndef MICHAELCC_PARSER_HPP
#define MICHAELCC_PARSER_HPP

#include <cstdint>
#include <unordered_map>
#include "tokens.hpp"
#include "ast.hpp"
#include "errors.hpp"

namespace michaelcc {
	class parser {
	private:
		struct declarator {
			std::unique_ptr<ast::ast_element> type;
			std::string identifier;
		};

		// Result being built (moved out at end of parse_all)
		std::vector<std::unique_ptr<ast::ast_element>> m_result;

		// Internal typedef tracking for type substitution
		std::unordered_map<std::string, const ast::ast_element*> m_typedefs;

		std::vector<token> m_tokens;
		int64_t m_token_index;
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

		uint8_t parse_storage_class();
		uint8_t parse_type_qualifiers();
		std::unique_ptr<ast::ast_element> parse_int_type();
		std::unique_ptr<ast::ast_element> parse_type(const bool parse_pointer=true);

		declarator parse_declarator();

		std::unique_ptr<ast::ast_element> parse_set_accessors(std::unique_ptr<ast::ast_element>&& initial_value);
		std::unique_ptr<ast::ast_element> parse_set_destination();

		std::unique_ptr<ast::ast_element> parse_value();
		std::unique_ptr<ast::ast_element> parse_expression(int min_precedence=0);

		std::unique_ptr<ast::ast_element> parse_statement();
		ast::context_block parse_block();

		ast::variable_declaration parse_variable_declaration();
		std::unique_ptr<ast::struct_declaration> parse_struct_declaration();
		std::unique_ptr<ast::union_declaration> parse_union_declaration();
		std::unique_ptr<ast::enum_declaration> parse_enum_declaration();
		std::unique_ptr<ast::typedef_declaration> parse_typedef_declaration();

		std::vector<ast::function_parameter> parse_parameter_list();
		std::unique_ptr<ast::function_prototype> parse_function_prototype();
		std::unique_ptr<ast::function_declaration> parse_function_declaration();

		const compilation_error panic(const std::string msg) const noexcept {
			return compilation_error(msg, current_loc);
		}

		// Check if a typedef exists
		bool has_typedef(const std::string& name) const {
			return m_typedefs.contains(name);
		}

		// Find a typedef - returns nullptr if not found
		const ast::ast_element* find_typedef(const std::string& name) const {
			auto it = m_typedefs.find(name);
			return it != m_typedefs.end() ? it->second : nullptr;
		}

	public:
		parser(std::vector<token>&& tokens) :
			m_tokens(std::move(tokens)), 
			m_token_index(-1),
			current_loc(0, 0, "invalid_file") { 
			next_token();
		}

		std::vector<std::unique_ptr<ast::ast_element>> parse_all();
	};
}

#endif
