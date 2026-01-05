#ifndef MICHAELCC_PARSER_HPP
#define MICHAELCC_PARSER_HPP

#include <cstdint>
#include <unordered_map>
#include "tokens.hpp"
#include "ast.hpp"
#include "errors.hpp"

namespace michaelcc {
	// Represents a complete parsed syntax tree (a single C source file after preprocessing).
	// Owns all AST nodes and provides fast lookups for named declarations.
	// All pointers in the lookup maps are valid for the lifetime of this object.
	class syntax_tree {
	private:
		std::vector<std::unique_ptr<ast::ast_element>> m_elements;
		std::unordered_map<std::string, ast::typedef_declaration*> m_typedefs;
		std::unordered_map<std::string, ast::struct_declaration*> m_structs;
		std::unordered_map<std::string, ast::enum_declaration*> m_enums;
		std::unordered_map<std::string, ast::union_declaration*> m_unions;

		// Only parser can construct
		friend class parser;
		syntax_tree() = default;

	public:
		// Move-only type
		syntax_tree(syntax_tree&&) = default;
		syntax_tree& operator=(syntax_tree&&) = default;
		syntax_tree(const syntax_tree&) = delete;
		syntax_tree& operator=(const syntax_tree&) = delete;

		// Access to top-level elements
		const std::vector<std::unique_ptr<ast::ast_element>>& elements() const noexcept { 
			return m_elements; 
		}

		// Lookup functions - return nullptr if not found
		ast::typedef_declaration* find_typedef(const std::string& name) const {
			auto it = m_typedefs.find(name);
			return it != m_typedefs.end() ? it->second : nullptr;
		}

		ast::struct_declaration* find_struct(const std::string& name) const {
			auto it = m_structs.find(name);
			return it != m_structs.end() ? it->second : nullptr;
		}

		ast::enum_declaration* find_enum(const std::string& name) const {
			auto it = m_enums.find(name);
			return it != m_enums.end() ? it->second : nullptr;
		}

		ast::union_declaration* find_union(const std::string& name) const {
			auto it = m_unions.find(name);
			return it != m_unions.end() ? it->second : nullptr;
		}

		// Check if declarations exist
		bool has_typedef(const std::string& name) const { return m_typedefs.contains(name); }
		bool has_struct(const std::string& name) const { return m_structs.contains(name); }
		bool has_enum(const std::string& name) const { return m_enums.contains(name); }
		bool has_union(const std::string& name) const { return m_unions.contains(name); }

		// Iteration support
		auto begin() const { return m_elements.begin(); }
		auto end() const { return m_elements.end(); }
		size_t size() const noexcept { return m_elements.size(); }
		bool empty() const noexcept { return m_elements.empty(); }
	};

	class parser {
	private:
		struct declarator {
			std::unique_ptr<ast::ast_element> type;
			std::string identifier;
		};

		// Result being built (moved out at end of parse_all)
		syntax_tree m_result;

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

		uint8_t parse_storage_qualifiers();
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
		// Helper to add a declaration to the result and register in lookup maps
		void add_typedef(std::unique_ptr<ast::typedef_declaration> decl);
		void add_struct(std::unique_ptr<ast::struct_declaration> decl);
		void add_enum(std::unique_ptr<ast::enum_declaration> decl);
		void add_union(std::unique_ptr<ast::union_declaration> decl);
		void add_element(std::unique_ptr<ast::ast_element> elem);

	public:
		parser(std::vector<token>&& tokens) :
			m_tokens(std::move(tokens)), 
			m_token_index(-1),
			current_loc(0, 0, "invalid_file") { 
			next_token();
		}

		syntax_tree parse_all();
	};
}

#endif
