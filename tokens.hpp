#pragma once

#include "errors.hpp"
#include <variant>
#include <string>
#include <memory>

namespace michaelcc {
	enum token_type {
		//preprocessor tokens
		MICHAELCC_PREPROCESSOR_TOKEN_DEFINE,
		MICHAELCC_PREPROCESSOR_TOKEN_IFDEF,
		MICHAELCC_PREPROCESSOR_TOKEN_IFNDEF,
		MICHAELCC_PREPROCESSOR_TOKEN_ELSE,
		MICHAELCC_PREPROCESSOR_TOKEN_ENDIF,
		MICHAELCC_PREPROCESSOR_TOKEN_INCLUDE,
		MICHAELCC_TOKEN_LINE_DIRECTIVE,

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
		MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR,
		MICHAELCC_TOKEN_PERIOD,
		MICHAELCC_TOKEN_TILDE,
		MICHAELCC_TOKEN_DEREFERENCE_GET,
		
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
		MICHAELCC_TOKEN_ASTERISK,
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
		std::variant<std::monostate, size_t, float, double, std::unique_ptr<std::string>, std::unique_ptr<source_location>> data;
		token_type m_type;
		uint32_t m_column;

	public:
		explicit token(token_type type, std::string str, size_t column) : m_type(type), data(std::make_unique<std::string>(str)), m_column(static_cast<uint32_t>(column)) {

		}

		explicit token(token_type type, size_t int_literal, size_t column) : m_type(type), data(int_literal), m_column(static_cast<uint32_t>(column)) {

		}

		explicit token(float f, size_t column) : m_type(MICHAELCC_TOKEN_FLOAT32_LITERAL), data(f), m_column(static_cast<uint32_t>(column)) {

		}

		explicit token(double d, size_t column) : m_type(MICHAELCC_TOKEN_FLOAT64_LITERAL), data(d), m_column(static_cast<uint32_t>(column)) {

		}

		explicit token(source_location location) : m_type(MICHAELCC_TOKEN_LINE_DIRECTIVE), data(std::make_unique<source_location>(location)), m_column(static_cast<uint32_t>(location.col())) {

		}

		explicit token(token_type type, size_t column) : m_type(type), data(std::monostate{}), m_column(static_cast<uint32_t>(column)) {

		}

		token(const token& to_copy) : m_type(to_copy.m_type), m_column(to_copy.m_column), data(std::monostate{}) {
			if (std::holds_alternative<size_t>(to_copy.data)) {
				data = to_copy.integer();
			} 
			else if (std::holds_alternative<double>(to_copy.data)) {
				data = to_copy.float64();
			}
			else if (std::holds_alternative<float>(to_copy.data)) {
				data = to_copy.float32();
			}
			else if (std::holds_alternative<std::unique_ptr<std::string>>(to_copy.data)) {
				data = std::make_unique<std::string>(to_copy.string());
			}
			else if (std::holds_alternative<std::unique_ptr<source_location>>(to_copy.data)) {
				data = std::make_unique<source_location>(to_copy.location());
			}
		}

		const token_type type() const noexcept {
			return m_type;
		}

		const std::string string() const {
			return *std::get<std::unique_ptr<std::string>>(data).get();
		}

		const float float32() const {
			return std::get<float>(data);
		}

		const double float64() const {
			return std::get<float>(data);
		}

		const size_t integer() const {
			return std::get<size_t>(data);
		}

		const source_location location() const {
			return *std::get<std::unique_ptr<source_location>>(data).get();
		}

		const uint32_t column() const noexcept {
			return m_column;
		}

		const bool is_preprocessor() const noexcept {
			return m_type >= MICHAELCC_PREPROCESSOR_TOKEN_DEFINE && m_type <= MICHAELCC_PREPROCESSOR_TOKEN_INCLUDE;
		}

		const bool is_operator() const noexcept {
			return m_type >= MICHAELCC_TOKEN_PLUS && m_type <= MICHAELCC_TOKEN_QUESTION;
		}

		const bool is_preprocessor_condition() const noexcept {
			return m_type >= MICHAELCC_PREPROCESSOR_TOKEN_IFDEF && m_type <= MICHAELCC_PREPROCESSOR_TOKEN_IFNDEF;
		}
	};

	const std::string token_to_str(token_type type);
}