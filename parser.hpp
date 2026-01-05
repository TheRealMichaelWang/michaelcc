#ifndef MICHAELCC_PARSER_HPP
#define MICHAELCC_PARSER_HPP

#include "tokens.hpp"
#include "syntax.hpp"
#include <vector>
#include <unordered_set>

namespace michaelcc {

class parser {
public:
	explicit parser(std::vector<token> tokens);
	syntax::translation_unit parse();

private:
	std::vector<token> m_tokens;
	size_t m_pos = 0;
	source_location m_loc;
	std::unordered_set<std::string> m_typedefs;

	bool at_end() const;
	const token& current() const;
	token_type peek() const;
	bool check(token_type t) const;
	bool match(token_type t);
	const token& advance();
	const token& expect(token_type t, const char* msg);
	source_location loc() const;
	[[noreturn]] void error(const char* msg);

	void skip_newlines();
	bool is_type_start() const;
	bool is_typename(const std::string& name) const;
	int precedence(token_type op) const;
	bool is_assign_op(token_type op) const;

	syntax::ptr parse_type();
	syntax::ptr parse_base_type();
	uint8_t parse_qualifiers();

	syntax::ptr parse_expr();
	syntax::ptr parse_assign();
	syntax::ptr parse_ternary();
	syntax::ptr parse_binary(int min_prec);
	syntax::ptr parse_unary();
	syntax::ptr parse_postfix();
	syntax::ptr parse_primary();
	std::vector<syntax::ptr> parse_args();

	syntax::ptr parse_stmt();
	std::unique_ptr<syntax::block> parse_block();
	syntax::ptr parse_if();
	syntax::ptr parse_while();
	syntax::ptr parse_do();
	syntax::ptr parse_for();
	syntax::ptr parse_switch();
	syntax::ptr parse_return();

	syntax::ptr parse_decl();
	syntax::ptr parse_var(uint8_t storage, syntax::ptr type, std::string name);
	syntax::ptr parse_func(syntax::ptr ret, std::string name);
	syntax::ptr parse_struct();
	syntax::ptr parse_union();
	syntax::ptr parse_enum();
	syntax::ptr parse_typedef();
	std::vector<syntax::param> parse_params();
};

}

#endif
