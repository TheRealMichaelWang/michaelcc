#include "parser.hpp"

namespace michaelcc {

using namespace syntax;

static const std::pair<token_type, int> PRECEDENCE[] = {
	{TOKEN_PIPE_PIPE, 1}, {TOKEN_AMP_AMP, 2}, {TOKEN_PIPE, 3}, {TOKEN_CARET, 4},
	{TOKEN_AMP, 5}, {TOKEN_EQ, 6}, {TOKEN_NE, 6}, {TOKEN_LT, 7}, {TOKEN_LE, 7},
	{TOKEN_GT, 7}, {TOKEN_GE, 7}, {TOKEN_LSHIFT, 8}, {TOKEN_RSHIFT, 8},
	{TOKEN_PLUS, 9}, {TOKEN_MINUS, 9}, {TOKEN_STAR, 10}, {TOKEN_SLASH, 10}, {TOKEN_PERCENT, 10}
};

parser::parser(std::vector<token> tokens) : m_tokens(std::move(tokens)) { skip_newlines(); }

bool parser::at_end() const { return m_pos >= m_tokens.size() || m_tokens[m_pos].type() == TOKEN_END; }

const token& parser::current() const {
	static token eof(TOKEN_END, 0);
	while (m_pos < m_tokens.size()) {
		const auto& t = m_tokens[m_pos];
		if (t.type() == TOKEN_LINE_DIRECTIVE || t.type() == TOKEN_NEWLINE) {
			if (t.type() == TOKEN_LINE_DIRECTIVE) const_cast<parser*>(this)->m_loc = t.loc();
			else const_cast<parser*>(this)->m_loc.next_line();
			++const_cast<parser*>(this)->m_pos;
			continue;
		}
		const_cast<parser*>(this)->m_loc.set_col(t.column());
		return t;
	}
	return eof;
}

token_type parser::peek() const { return current().type(); }
bool parser::check(token_type t) const { return peek() == t; }
source_location parser::loc() const { return m_loc; }
void parser::error(const char* msg) { throw compile_error(msg, m_loc); }

bool parser::match(token_type t) {
	if (check(t)) { advance(); return true; }
	return false;
}

const token& parser::advance() {
	while (!at_end()) {
		const token& t = m_tokens[m_pos++];
		if (t.type() == TOKEN_LINE_DIRECTIVE) { m_loc = t.loc(); continue; }
		if (t.type() == TOKEN_NEWLINE) { m_loc.next_line(); continue; }
		m_loc.set_col(t.column());
		return t;
	}
	return m_tokens.back();
}

const token& parser::expect(token_type t, const char* msg) {
	if (!check(t)) error(msg);
	return advance();
}

void parser::skip_newlines() {
	while (!at_end()) {
		if (m_tokens[m_pos].type() == TOKEN_LINE_DIRECTIVE) { m_loc = m_tokens[m_pos++].loc(); }
		else if (m_tokens[m_pos].type() == TOKEN_NEWLINE) { m_loc.next_line(); ++m_pos; }
		else { m_loc.set_col(m_tokens[m_pos].column()); break; }
	}
}

bool parser::is_typename(const std::string& n) const { return m_typedefs.contains(n); }

bool parser::is_type_start() const {
	switch (peek()) {
	case TOKEN_VOID: case TOKEN_CHAR: case TOKEN_SHORT: case TOKEN_INT: case TOKEN_LONG:
	case TOKEN_FLOAT: case TOKEN_DOUBLE: case TOKEN_SIGNED: case TOKEN_UNSIGNED:
	case TOKEN_STRUCT: case TOKEN_UNION: case TOKEN_ENUM:
	case TOKEN_CONST: case TOKEN_VOLATILE: case TOKEN_EXTERN: case TOKEN_STATIC: case TOKEN_REGISTER:
		return true;
	case TOKEN_IDENTIFIER: return is_typename(current().str());
	default: return false;
	}
}

int parser::precedence(token_type op) const {
	for (auto& [t, p] : PRECEDENCE) if (t == op) return p;
	return -1;
}

bool parser::is_assign_op(token_type op) const {
	return op == TOKEN_ASSIGN || (op >= TOKEN_PLUS_ASSIGN && op <= TOKEN_RSHIFT_ASSIGN);
}

uint8_t parser::parse_qualifiers() {
	uint8_t q = 0;
	for (;;) {
		if (match(TOKEN_CONST)) q |= 0x01;
		else if (match(TOKEN_VOLATILE)) q |= 0x02;
		else if (match(TOKEN_RESTRICT)) q |= 0x04;
		else if (match(TOKEN_EXTERN)) q |= 0x08;
		else if (match(TOKEN_STATIC)) q |= 0x10;
		else if (match(TOKEN_REGISTER)) q |= 0x20;
		else break;
	}
	return q;
}

ptr parser::parse_base_type() {
	auto l = loc();
	parse_qualifiers();

	if (match(TOKEN_VOID)) return std::make_unique<void_type>(l);

	if (match(TOKEN_STRUCT)) {
		if (check(TOKEN_IDENTIFIER)) return std::make_unique<named_type>(named_type::kind::STRUCT, advance().str(), l);
		error("expected struct name");
	}
	if (match(TOKEN_UNION)) {
		if (check(TOKEN_IDENTIFIER)) return std::make_unique<named_type>(named_type::kind::UNION, advance().str(), l);
		error("expected union name");
	}
	if (match(TOKEN_ENUM)) {
		if (check(TOKEN_IDENTIFIER)) return std::make_unique<named_type>(named_type::kind::ENUM, advance().str(), l);
		error("expected enum name");
	}

	if (check(TOKEN_IDENTIFIER) && is_typename(current().str()))
		return std::make_unique<named_type>(named_type::kind::TYPEDEF, advance().str(), l);

	bool is_signed = false, is_unsigned = false, has_short = false, has_char = false, has_int = false;
	int longs = 0;

	for (;;) {
		if (match(TOKEN_SIGNED)) is_signed = true;
		else if (match(TOKEN_UNSIGNED)) is_unsigned = true;
		else if (match(TOKEN_LONG)) ++longs;
		else if (match(TOKEN_SHORT)) has_short = true;
		else if (match(TOKEN_CHAR)) has_char = true;
		else if (match(TOKEN_INT)) has_int = true;
		else break;
	}

	if (has_char || has_short || has_int || longs || is_signed || is_unsigned) {
		int_type::kind k = has_char ? int_type::kind::CHAR : has_short ? int_type::kind::SHORT : longs ? int_type::kind::LONG : int_type::kind::INT;
		uint8_t q = 0;
		if (is_unsigned) q |= 0x08; else if (is_signed) q |= 0x04;
		if (longs) q |= 0x01;
		return std::make_unique<int_type>(q, k, l);
	}

	if (match(TOKEN_FLOAT)) return std::make_unique<float_type>(float_type::kind::FLOAT, l);
	if (match(TOKEN_DOUBLE)) return std::make_unique<float_type>(float_type::kind::DOUBLE, l);

	error("expected type");
}

ptr parser::parse_type() {
	auto base = parse_base_type();
	auto l = loc();
	while (match(TOKEN_STAR)) {
		parse_qualifiers();
		base = std::make_unique<pointer_type>(std::move(base), l);
	}
	return base;
}

ptr parser::parse_expr() { return parse_assign(); }

ptr parser::parse_assign() {
	auto e = parse_ternary();
	if (is_assign_op(peek())) {
		auto op = advance().type();
		return std::make_unique<assign>(op, std::move(e), parse_assign(), loc());
	}
	return e;
}

ptr parser::parse_ternary() {
	auto e = parse_binary(1);
	if (match(TOKEN_QUESTION)) {
		auto t = parse_expr();
		expect(TOKEN_COLON, "expected ':'");
		return std::make_unique<ternary>(std::move(e), std::move(t), parse_ternary(), loc());
	}
	return e;
}

ptr parser::parse_binary(int min_prec) {
	auto left = parse_unary();
	for (;;) {
		int prec = precedence(peek());
		if (prec < min_prec) break;
		auto op = advance().type();
		left = std::make_unique<binary>(op, std::move(left), parse_binary(prec + 1), loc());
	}
	return left;
}

ptr parser::parse_unary() {
	auto l = loc();
	if (match(TOKEN_INC)) return std::make_unique<unary>(TOKEN_INC, parse_unary(), true, l);
	if (match(TOKEN_DEC)) return std::make_unique<unary>(TOKEN_DEC, parse_unary(), true, l);
	if (match(TOKEN_MINUS)) return std::make_unique<unary>(TOKEN_MINUS, parse_unary(), true, l);
	if (match(TOKEN_BANG)) return std::make_unique<unary>(TOKEN_BANG, parse_unary(), true, l);
	if (match(TOKEN_TILDE)) return std::make_unique<unary>(TOKEN_TILDE, parse_unary(), true, l);
	if (match(TOKEN_STAR)) return std::make_unique<unary>(TOKEN_STAR, parse_unary(), true, l);
	if (match(TOKEN_AMP)) return std::make_unique<unary>(TOKEN_AMP, parse_unary(), true, l);

	if (match(TOKEN_SIZEOF)) {
		if (match(TOKEN_LPAREN)) {
			if (is_type_start()) {
				auto t = parse_type();
				expect(TOKEN_RPAREN, "expected ')'");
				return std::make_unique<sizeof_expr>(std::move(t), true, l);
			}
			auto e = parse_expr();
			expect(TOKEN_RPAREN, "expected ')'");
			return std::make_unique<sizeof_expr>(std::move(e), false, l);
		}
		return std::make_unique<sizeof_expr>(parse_unary(), false, l);
	}

	if (check(TOKEN_LPAREN)) {
		size_t saved = m_pos;
		advance();
		if (is_type_start()) {
			auto t = parse_type();
			if (match(TOKEN_RPAREN)) {
				if (!check(TOKEN_LBRACE))
					return std::make_unique<cast>(std::move(t), parse_unary(), l);
			}
		}
		m_pos = saved;
		skip_newlines();
	}

	return parse_postfix();
}

ptr parser::parse_postfix() {
	auto e = parse_primary();
	for (;;) {
		auto l = loc();
		if (match(TOKEN_LPAREN)) {
			auto args = parse_args();
			expect(TOKEN_RPAREN, "expected ')'");
			e = std::make_unique<call>(std::move(e), std::move(args), l);
		} else if (match(TOKEN_LBRACKET)) {
			auto idx = parse_expr();
			expect(TOKEN_RBRACKET, "expected ']'");
			e = std::make_unique<index>(std::move(e), std::move(idx), l);
		} else if (match(TOKEN_DOT)) {
			e = std::make_unique<member>(std::move(e), expect(TOKEN_IDENTIFIER, "expected member").str(), false, l);
		} else if (match(TOKEN_ARROW)) {
			e = std::make_unique<member>(std::move(e), expect(TOKEN_IDENTIFIER, "expected member").str(), true, l);
		} else if (match(TOKEN_INC)) {
			e = std::make_unique<unary>(TOKEN_INC, std::move(e), false, l);
		} else if (match(TOKEN_DEC)) {
			e = std::make_unique<unary>(TOKEN_DEC, std::move(e), false, l);
		} else break;
	}
	return e;
}

ptr parser::parse_primary() {
	auto l = loc();

	if (check(TOKEN_INT_LITERAL)) return std::make_unique<int_lit>(advance().integer(), 0, int_type::kind::INT, l);
	if (check(TOKEN_CHAR_LITERAL)) return std::make_unique<int_lit>(advance().integer(), 0, int_type::kind::CHAR, l);
	if (check(TOKEN_FLOAT_LITERAL)) return std::make_unique<float_lit>(advance().num(), float_type::kind::FLOAT, l);
	if (check(TOKEN_DOUBLE_LITERAL)) return std::make_unique<float_lit>(advance().num(), float_type::kind::DOUBLE, l);
	if (check(TOKEN_STRING_LITERAL)) return std::make_unique<string_lit>(advance().str(), l);
	if (check(TOKEN_IDENTIFIER)) return std::make_unique<ident>(advance().str(), l);

	if (match(TOKEN_LPAREN)) {
		auto e = parse_expr();
		expect(TOKEN_RPAREN, "expected ')'");
		return e;
	}

	if (match(TOKEN_LBRACE)) {
		std::vector<ptr> elems;
		if (!check(TOKEN_RBRACE)) {
			do { elems.push_back(parse_assign()); } while (match(TOKEN_COMMA) && !check(TOKEN_RBRACE));
		}
		expect(TOKEN_RBRACE, "expected '}'");
		return std::make_unique<init_list>(std::move(elems), l);
	}

	error("expected expression");
}

std::vector<ptr> parser::parse_args() {
	std::vector<ptr> args;
	if (!check(TOKEN_RPAREN)) {
		do { args.push_back(parse_assign()); } while (match(TOKEN_COMMA));
	}
	return args;
}

ptr parser::parse_stmt() {
	skip_newlines();
	auto l = loc();

	if (check(TOKEN_LBRACE)) return parse_block();
	if (check(TOKEN_IF)) return parse_if();
	if (check(TOKEN_WHILE)) return parse_while();
	if (check(TOKEN_DO)) return parse_do();
	if (check(TOKEN_FOR)) return parse_for();
	if (check(TOKEN_SWITCH)) return parse_switch();
	if (check(TOKEN_RETURN)) return parse_return();
	if (match(TOKEN_BREAK)) { expect(TOKEN_SEMICOLON, "expected ';'"); return std::make_unique<break_stmt>(l); }
	if (match(TOKEN_CONTINUE)) { expect(TOKEN_SEMICOLON, "expected ';'"); return std::make_unique<continue_stmt>(l); }
	if (match(TOKEN_GOTO)) {
		auto label = expect(TOKEN_IDENTIFIER, "expected label").str();
		expect(TOKEN_SEMICOLON, "expected ';'");
		return std::make_unique<goto_stmt>(std::move(label), l);
	}
	if (match(TOKEN_SEMICOLON)) return std::make_unique<empty_stmt>(l);

	if (is_type_start()) return parse_decl();

	if (check(TOKEN_IDENTIFIER)) {
		size_t saved = m_pos;
		auto name = advance().str();
		if (match(TOKEN_COLON)) return std::make_unique<label_stmt>(std::move(name), parse_stmt(), l);
		m_pos = saved;
		skip_newlines();
	}

	auto e = parse_expr();
	expect(TOKEN_SEMICOLON, "expected ';'");
	return std::make_unique<expr_stmt>(std::move(e), l);
}

std::unique_ptr<block> parser::parse_block() {
	auto l = loc();
	expect(TOKEN_LBRACE, "expected '{'");
	std::vector<ptr> stmts;
	for (;;) {
		skip_newlines();
		if (check(TOKEN_RBRACE) || at_end()) break;
		stmts.push_back(parse_stmt());
	}
	expect(TOKEN_RBRACE, "expected '}'");
	return std::make_unique<block>(std::move(stmts), l);
}

ptr parser::parse_if() {
	auto l = loc();
	expect(TOKEN_IF, "expected 'if'");
	expect(TOKEN_LPAREN, "expected '('");
	auto cond = parse_expr();
	expect(TOKEN_RPAREN, "expected ')'");
	auto then_b = parse_stmt();
	ptr else_b;
	if (match(TOKEN_ELSE)) else_b = parse_stmt();
	return std::make_unique<if_stmt>(std::move(cond), std::move(then_b), std::move(else_b), l);
}

ptr parser::parse_while() {
	auto l = loc();
	expect(TOKEN_WHILE, "expected 'while'");
	expect(TOKEN_LPAREN, "expected '('");
	auto cond = parse_expr();
	expect(TOKEN_RPAREN, "expected ')'");
	return std::make_unique<while_stmt>(std::move(cond), parse_stmt(), l);
}

ptr parser::parse_do() {
	auto l = loc();
	expect(TOKEN_DO, "expected 'do'");
	auto body = parse_stmt();
	expect(TOKEN_WHILE, "expected 'while'");
	expect(TOKEN_LPAREN, "expected '('");
	auto cond = parse_expr();
	expect(TOKEN_RPAREN, "expected ')'");
	expect(TOKEN_SEMICOLON, "expected ';'");
	return std::make_unique<do_stmt>(std::move(body), std::move(cond), l);
}

ptr parser::parse_for() {
	auto l = loc();
	expect(TOKEN_FOR, "expected 'for'");
	expect(TOKEN_LPAREN, "expected '('");

	ptr init;
	if (!check(TOKEN_SEMICOLON)) {
		if (is_type_start()) init = parse_decl();
		else { init = std::make_unique<expr_stmt>(parse_expr(), loc()); expect(TOKEN_SEMICOLON, "expected ';'"); }
	} else { advance(); init = std::make_unique<empty_stmt>(loc()); }

	ptr cond;
	if (!check(TOKEN_SEMICOLON)) cond = parse_expr();
	expect(TOKEN_SEMICOLON, "expected ';'");

	ptr inc;
	if (!check(TOKEN_RPAREN)) inc = parse_expr();
	expect(TOKEN_RPAREN, "expected ')'");

	return std::make_unique<for_stmt>(std::move(init), std::move(cond), std::move(inc), parse_stmt(), l);
}

ptr parser::parse_switch() {
	auto l = loc();
	expect(TOKEN_SWITCH, "expected 'switch'");
	expect(TOKEN_LPAREN, "expected '('");
	auto e = parse_expr();
	expect(TOKEN_RPAREN, "expected ')'");
	expect(TOKEN_LBRACE, "expected '{'");

	std::vector<switch_case> cases;
	while (!check(TOKEN_RBRACE) && !at_end()) {
		switch_case c;
		if (match(TOKEN_CASE)) {
			c.val = parse_expr();
			expect(TOKEN_COLON, "expected ':'");
		} else if (match(TOKEN_DEFAULT)) {
			expect(TOKEN_COLON, "expected ':'");
		} else error("expected 'case' or 'default'");

		while (!check(TOKEN_CASE) && !check(TOKEN_DEFAULT) && !check(TOKEN_RBRACE) && !at_end())
			c.stmts.push_back(parse_stmt());
		cases.push_back(std::move(c));
	}
	expect(TOKEN_RBRACE, "expected '}'");
	return std::make_unique<switch_stmt>(std::move(e), std::move(cases), l);
}

ptr parser::parse_return() {
	auto l = loc();
	expect(TOKEN_RETURN, "expected 'return'");
	ptr val;
	if (!check(TOKEN_SEMICOLON)) val = parse_expr();
	expect(TOKEN_SEMICOLON, "expected ';'");
	return std::make_unique<return_stmt>(std::move(val), l);
}

std::vector<param> parser::parse_params() {
	std::vector<param> params;
	expect(TOKEN_LPAREN, "expected '('");

	if (match(TOKEN_VOID)) {
		if (check(TOKEN_RPAREN)) { expect(TOKEN_RPAREN, "expected ')'"); return params; }
		error("expected ')' after void");
	}

	if (!check(TOKEN_RPAREN)) {
		do {
			param p;
			p.quals = parse_qualifiers();
			p.type = parse_type();
			if (check(TOKEN_IDENTIFIER)) p.name = advance().str();
			while (match(TOKEN_LBRACKET)) {
				ptr sz;
				if (!check(TOKEN_RBRACKET)) sz = parse_expr();
				expect(TOKEN_RBRACKET, "expected ']'");
				p.type = std::make_unique<array_type>(std::move(p.type), std::move(sz), loc());
			}
			params.push_back(std::move(p));
		} while (match(TOKEN_COMMA));
	}
	expect(TOKEN_RPAREN, "expected ')'");
	return params;
}

ptr parser::parse_decl() {
	auto l = loc();
	uint8_t storage = parse_qualifiers();

	if (match(TOKEN_TYPEDEF)) return parse_typedef();
	if (match(TOKEN_STRUCT)) { auto d = parse_struct(); expect(TOKEN_SEMICOLON, "expected ';'"); return d; }
	if (match(TOKEN_UNION)) { auto d = parse_union(); expect(TOKEN_SEMICOLON, "expected ';'"); return d; }
	if (match(TOKEN_ENUM)) { auto d = parse_enum(); expect(TOKEN_SEMICOLON, "expected ';'"); return d; }

	auto type = parse_type();
	auto name = expect(TOKEN_IDENTIFIER, "expected identifier").str();

	if (check(TOKEN_LPAREN)) return parse_func(std::move(type), std::move(name));
	return parse_var(storage, std::move(type), std::move(name));
}

ptr parser::parse_var(uint8_t storage, ptr type, std::string name) {
	auto l = loc();
	while (match(TOKEN_LBRACKET)) {
		ptr sz;
		if (!check(TOKEN_RBRACKET)) sz = parse_expr();
		expect(TOKEN_RBRACKET, "expected ']'");
		type = std::make_unique<array_type>(std::move(type), std::move(sz), loc());
	}
	ptr init;
	if (match(TOKEN_ASSIGN)) init = parse_assign();
	expect(TOKEN_SEMICOLON, "expected ';'");
	return std::make_unique<var_decl>(storage, std::move(type), std::move(name), std::move(init), l);
}

ptr parser::parse_func(ptr ret, std::string name) {
	auto l = loc();
	auto params = parse_params();
	std::unique_ptr<block> body;
	if (check(TOKEN_LBRACE)) body = parse_block();
	else expect(TOKEN_SEMICOLON, "expected ';' or '{'");
	return std::make_unique<func_decl>(std::move(ret), std::move(name), std::move(params), std::move(body), l);
}

ptr parser::parse_struct() {
	auto l = loc();
	std::optional<std::string> name;
	if (check(TOKEN_IDENTIFIER)) name = advance().str();

	std::vector<struct_field> fields;
	if (match(TOKEN_LBRACE)) {
		while (!check(TOKEN_RBRACE) && !at_end()) {
			auto t = parse_type();
			auto n = expect(TOKEN_IDENTIFIER, "expected field name").str();
			while (match(TOKEN_LBRACKET)) {
				ptr sz;
				if (!check(TOKEN_RBRACKET)) sz = parse_expr();
				expect(TOKEN_RBRACKET, "expected ']'");
				t = std::make_unique<array_type>(std::move(t), std::move(sz), loc());
			}
			ptr bits;
			if (match(TOKEN_COLON)) bits = parse_expr();
			fields.push_back({std::move(t), std::move(n), std::move(bits)});
			expect(TOKEN_SEMICOLON, "expected ';'");
		}
		expect(TOKEN_RBRACE, "expected '}'");
	}
	return std::make_unique<struct_decl>(std::move(name), std::move(fields), l);
}

ptr parser::parse_union() {
	auto l = loc();
	std::optional<std::string> name;
	if (check(TOKEN_IDENTIFIER)) name = advance().str();

	std::vector<struct_field> fields;
	if (match(TOKEN_LBRACE)) {
		while (!check(TOKEN_RBRACE) && !at_end()) {
			auto t = parse_type();
			auto n = expect(TOKEN_IDENTIFIER, "expected field name").str();
			fields.push_back({std::move(t), std::move(n), nullptr});
			expect(TOKEN_SEMICOLON, "expected ';'");
		}
		expect(TOKEN_RBRACE, "expected '}'");
	}
	return std::make_unique<union_decl>(std::move(name), std::move(fields), l);
}

ptr parser::parse_enum() {
	auto l = loc();
	std::optional<std::string> name;
	if (check(TOKEN_IDENTIFIER)) name = advance().str();

	std::vector<enumerator> values;
	if (match(TOKEN_LBRACE)) {
		do {
			auto n = expect(TOKEN_IDENTIFIER, "expected enumerator").str();
			ptr v;
			if (match(TOKEN_ASSIGN)) v = parse_expr();
			values.push_back({std::move(n), std::move(v)});
		} while (match(TOKEN_COMMA) && !check(TOKEN_RBRACE));
		expect(TOKEN_RBRACE, "expected '}'");
	}
	return std::make_unique<enum_decl>(std::move(name), std::move(values), l);
}

ptr parser::parse_typedef() {
	auto l = loc();
	ptr t;

	if (check(TOKEN_STRUCT)) {
		advance();
		t = parse_struct();
	} else if (check(TOKEN_UNION)) {
		advance();
		t = parse_union();
	} else if (check(TOKEN_ENUM)) {
		advance();
		t = parse_enum();
	} else {
		t = parse_type();
	}

	auto name = expect(TOKEN_IDENTIFIER, "expected typedef name").str();
	m_typedefs.insert(name);
	expect(TOKEN_SEMICOLON, "expected ';'");
	return std::make_unique<typedef_decl>(std::move(t), std::move(name), l);
}

translation_unit parser::parse() {
	std::vector<ptr> decls;
	while (!at_end()) {
		skip_newlines();
		if (at_end()) break;
		decls.push_back(parse_decl());
	}
	return translation_unit(std::move(decls), loc());
}

}
