#include "tokens.hpp"

using namespace michaelcc;

const std::string michaelcc::token_to_str(token_type type) {
	static const char* tok_names[] = {
		//preprocessor tokens
		"DEFINE",
		"IFDEF",
		"IFNDEF",
		"ELSE",
		"ENDIF",
		"INCLUDE",

		//punctuators
		"OPEN_BRACKET",
		"CLOSE_BRACKET",
		"OPEN_PAREN",
		"CLOSE_PAREN",
		"OPEN_BRACE",
		"CLOSE_BRACE",
		"COMMA",
		"COLON",
		"SEMICOLON",
		"ASTERISK",
		"ASSIGNMENT_OPERATOR",
		"PERIOD",
		"TILDE",

		//keywords
		"AUTO",
		"BREAK",
		"CASE",
		"CHAR",
		"CONST",
		"CONTINUE",
		"DEFAULT",
		"DO",
		"DOUBLE",
		"ELSE",
		"ENUM",
		"EXTERN",
		"FLOAT",
		"FOR",
		"GOTO",
		"IF",
		"INT",
		"LONG",
		"REGISTER",
		"RETURN",
		"SHORT",
		"SIGNED",
		"SIZEOF",
		"STATIC",
		"STRUCT",
		"SWITCH",
		"TYPEDEF",
		"UNION",
		"UNSIGNED",
		"VOID",
		"VOLATILE",
		"WHILE",
		"IDENTIFIER",

		//operators
		"PLUS",
		"MINUS",
		"INCREMENT",
		"DECREMENT",
		"INCREMENT_BY",
		"DECREMENT_BY",
		"SLASH",
		"CARET",
		"AND",
		"OR",
		"DOUBLE_AND",
		"DOUBLE_OR",
		"MORE",
		"LESS",
		"MORE_EQUAL",
		"LESS_EQUAL",
		"EQUALS",
		"QUESTION",

		//literal tokens
		"INTEGER_LITERAL",
		"CHAR_LITERAL",
		"FLOAT32_LITERAL",
		"FLOAT64_LITERAL",
		"STRING_LITERAL",

		//control tokens
		"NEWLINE",
		"END"
	};
	return tok_names[type];
}