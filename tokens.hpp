#pragma once

#include <variant>
#include <string>

namespace michaelcc {
	enum token_type {
		//preprocessor tokens
		MICHAELCC_PREPROCESSOR_TOKEN_DEFINE,
		MICHAELCC_PREPROCESSOR_TOKEN_IFDEF,
		MICHAELCC_PREPROCESSOR_TOKEN_IFNDEF,
		MICHAELCC_PREPROCESSOR_TOKEN_ELSE,
		MICHAELCC_PREPROCESSOR_TOKEN_ENDIF,
		MICHAELCC_PREPROCESSOR_TOKEN_INCLUDE,

		//punctuators
		MICHAELCC_TOKEN_OPEN_BRACKET,
		MICHAELCC_TOKEN_CLOSE_BRACKET,
		MICHAELCC_TOKEN_OPEN_PAREN,
		MICHAELCC_TOKEN_CLOSE_PAREN,
		MICHAELCC_TOKEN_OPEN_BRACE,
		MICHAELCC_TOKEN_CLOSE_BRACE,
		MICHAELCC_TOKEN_COMMA,
		MICHAELCC_TOKEN_COLON,
		MICHAELCC_TOKEN_SEMICOLON,
		MICHAELCC_TOKEN_ASTERISK,
		MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR,
		MICHAELCC_TOKEN_PERIOD,
		MICHAELCC_TOKEN_TILDE,
		
		//keywords
		MICHAELCC_TOKEN_AUTO,
		MICHAELCC_TOKEN_BREAK,
		MICHAELCC_TOKEN_CASE,
		MICHAELCC_TOKEN_CHAR,
		MICHAELCC_TOKEN_CONST,
		MICHAELCC_TOKEN_CONTINUE,
		MICHAELCC_TOKEN_DEFAULT,
		MICHAELCC_TOKEN_DO,
		MICHAELCC_TOKEN_DOUBLE,
		MICHAELCC_TOKEN_ELSE,
		MICHAELCC_TOKEN_ENUM,
		MICHAELCC_TOKEN_EXTERN,
		MICHAELCC_TOKEN_FLOAT,
		MICHAELCC_TOKEN_FOR,
		MICHAELCC_TOKEN_GOTO,
		MICHAELCC_TOKEN_IF,
		MICHAELCC_TOKEN_INT,
		MICHAELCC_TOKEN_LONG,
		MICHAELCC_TOKEN_REGISTER,
		MICHAELCC_TOKEN_RETURN,
		MICHAELCC_TOKEN_SHORT,
		MICHAELCC_TOKEN_SIGNED,
		MICHAELCC_TOKEN_SIZEOF,
		MICHAELCC_TOKEN_STATIC,
		MICHAELCC_TOKEN_STRUCT,
		MICHAELCC_TOKEN_SWITCH,
		MICHAELCC_TOKEN_TYPEDEF,
		MICHAELCC_TOKEN_UNION,
		MICHAELCC_TOKEN_UNSIGNED,
		MICHAELCC_TOKEN_VOID,
		MICHAELCC_TOKEN_VOLATILE,
		MICHAELCC_TOKEN_WHILE,
		MICHAELCC_TOKEN_IDENTIFIER,

		//operators
		MICHAELCC_TOKEN_PLUS,
		MICHAELCC_TOKEN_MINUS,
		MICHAELCC_TOKEN_INCREMENT,
		MICHAELCC_TOKEN_DECREMENT,
		MICHAELCC_TOKEN_INCREMENT_BY,
		MICHAELCC_TOKEN_DECREMENT_BY,
		MICHAELCC_TOKEN_SLASH,
		MICHAELCC_TOKEN_CARET,
		MICHAELCC_TOKEN_AND,
		MICHAELCC_TOKEN_OR,
		MICHAELCC_TOKEN_DOUBLE_AND,
		MICHAELCC_TOKEN_DOUBLE_OR,
		MICHAELCC_TOKEN_MORE,
		MICHAELCC_TOKEN_LESS,
		MICHAELCC_TOKEN_MORE_EQUAL,
		MICHAELCC_TOKEN_LESS_EQUAL,
		MICHAELCC_TOKEN_EQUALS,
		MICHAELCC_TOKEN_QUESTION,

		//literal tokens
		MICHAELCC_TOKEN_INTEGER_LITERAL,
		MICHAELCC_TOKEN_CHAR_LITERAL,
		MICHAELCC_TOKEN_FLOAT32_LITERAL,
		MICHAELCC_TOKEN_FLOAT64_LITERAL,
		MICHAELCC_TOKEN_STRING_LITERAL,

		//control tokens
		MICHAELCC_TOKEN_NEWLINE,
		MICHAELCC_TOKEN_END
	};

	class token {
	private:
		token_type m_type;
		std::variant<std::string, size_t, float, double, std::monostate> data;

	public:
		token(token_type type, std::string str) : m_type(type), data(str) {

		}

		token(token_type type, size_t int_literal) : m_type(type), data(int_literal) {

		}

		token(float f) : m_type(MICHAELCC_TOKEN_FLOAT32_LITERAL), data(f) {

		}

		token(double d) : m_type(MICHAELCC_TOKEN_FLOAT64_LITERAL), data(d) {

		}

		token(token_type type) : m_type(type), data(std::monostate{}) {

		}

		const token_type type() const noexcept {
			return m_type;
		}

		const std::string string() const {
			return std::get<std::string>(data);
		}

		const float float32() const {
			return std::get<float>(data);
		}

		const double float64() const {
			return std::get<float>(data);
		}

		const bool is_preprocessor() const noexcept {
			return m_type >= MICHAELCC_PREPROCESSOR_TOKEN_DEFINE && m_type <= MICHAELCC_PREPROCESSOR_TOKEN_INCLUDE;
		}

		const bool is_preprocessor_condition() const noexcept {
			return m_type >= MICHAELCC_PREPROCESSOR_TOKEN_IFDEF && m_type <= MICHAELCC_PREPROCESSOR_TOKEN_IFNDEF;
		}
	};

	const std::string token_to_str(token_type type);
}