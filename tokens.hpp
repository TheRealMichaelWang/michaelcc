#ifndef MICHAELCC_TOKENS_HPP
#define MICHAELCC_TOKENS_HPP

#include "errors.hpp"
#include <variant>
#include <string>

namespace michaelcc {

enum token_type {
	TOKEN_PP_DEFINE, TOKEN_PP_IFDEF, TOKEN_PP_IFNDEF, TOKEN_PP_ELSE,
	TOKEN_PP_ENDIF, TOKEN_PP_INCLUDE, TOKEN_LINE_DIRECTIVE, TOKEN_PP_STRINGIFY,

	TOKEN_LBRACKET, TOKEN_RBRACKET, TOKEN_LPAREN, TOKEN_RPAREN,
	TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_COMMA, TOKEN_COLON, TOKEN_SEMICOLON,
	TOKEN_ASSIGN, TOKEN_DOT, TOKEN_TILDE, TOKEN_ARROW,

	TOKEN_AUTO, TOKEN_BREAK, TOKEN_CASE, TOKEN_CHAR, TOKEN_CONST,
	TOKEN_CONTINUE, TOKEN_DEFAULT, TOKEN_DO, TOKEN_DOUBLE, TOKEN_ELSE,
	TOKEN_ENUM, TOKEN_EXTERN, TOKEN_FLOAT, TOKEN_FOR, TOKEN_GOTO,
	TOKEN_IF, TOKEN_INT, TOKEN_LONG, TOKEN_REGISTER, TOKEN_RETURN,
	TOKEN_SHORT, TOKEN_SIGNED, TOKEN_SIZEOF, TOKEN_STATIC, TOKEN_STRUCT,
	TOKEN_SWITCH, TOKEN_TYPEDEF, TOKEN_UNION, TOKEN_UNSIGNED, TOKEN_VOID,
	TOKEN_VOLATILE, TOKEN_RESTRICT, TOKEN_WHILE, TOKEN_IDENTIFIER,

	TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_INC, TOKEN_DEC,
	TOKEN_PLUS_ASSIGN, TOKEN_MINUS_ASSIGN, TOKEN_SLASH, TOKEN_CARET,
	TOKEN_AMP, TOKEN_PIPE, TOKEN_AMP_AMP, TOKEN_PIPE_PIPE,
	TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE, TOKEN_EQ, TOKEN_QUESTION,
	TOKEN_PERCENT, TOKEN_BANG, TOKEN_NE, TOKEN_LSHIFT, TOKEN_RSHIFT,
	TOKEN_STAR_ASSIGN, TOKEN_SLASH_ASSIGN, TOKEN_PERCENT_ASSIGN,
	TOKEN_AMP_ASSIGN, TOKEN_PIPE_ASSIGN, TOKEN_CARET_ASSIGN,
	TOKEN_LSHIFT_ASSIGN, TOKEN_RSHIFT_ASSIGN,

	TOKEN_INT_LITERAL, TOKEN_CHAR_LITERAL, TOKEN_FLOAT_LITERAL,
	TOKEN_DOUBLE_LITERAL, TOKEN_STRING_LITERAL,

	TOKEN_NEWLINE, TOKEN_END
};

class token {
public:
	using data_t = std::variant<std::monostate, uint64_t, double, std::string, source_location>;

private:
	token_type m_type;
	data_t m_data;
	uint32_t m_col;

public:
	token(token_type type, uint32_t col) : m_type(type), m_col(col) {}
	token(token_type type, std::string s, uint32_t col) : m_type(type), m_data(std::move(s)), m_col(col) {}
	token(token_type type, uint64_t v, uint32_t col) : m_type(type), m_data(v), m_col(col) {}
	token(token_type type, double v, uint32_t col) : m_type(type), m_data(v), m_col(col) {}
	explicit token(source_location loc) : m_type(TOKEN_LINE_DIRECTIVE), m_data(std::move(loc)), m_col(0) {}

	token(token&&) = default;
	token& operator=(token&&) = default;
	token(const token&) = default;
	token& operator=(const token&) = default;

	token_type type() const noexcept { return m_type; }
	uint32_t column() const noexcept { return m_col; }

	const std::string& str() const { return std::get<std::string>(m_data); }
	double num() const { return std::get<double>(m_data); }
	uint64_t integer() const { return std::get<uint64_t>(m_data); }
	const source_location& loc() const { return std::get<source_location>(m_data); }

	bool is_pp() const noexcept { return m_type >= TOKEN_PP_DEFINE && m_type <= TOKEN_PP_INCLUDE; }
	bool is_op() const noexcept { return m_type >= TOKEN_PLUS && m_type <= TOKEN_RSHIFT_ASSIGN; }
	bool is_pp_cond() const noexcept { return m_type == TOKEN_PP_IFDEF || m_type == TOKEN_PP_IFNDEF; }
};

std::string token_type_str(token_type type);
std::string token_str(const token& tok);

}

#endif
