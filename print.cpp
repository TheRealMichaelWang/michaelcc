#include "tokens.hpp"

using namespace michaelcc;

const std::string michaelcc::token_to_str(token_type type) {
	static const char* tok_names[] = {
		//preprocessor tokens
		"#define",
		"#ifdef",
		"#ifndef",
		"#else",
		"#endif",
		"#include",
		"#line",

		//punctuators
		"[",
		"]",
		"(",
		")",
		"{",
		"}",
		",",
		":",
		";",
		"*",
		"=",
		".",
		"~",
		"->",

		//keywords
		"auto",
		"break",
		"case",
		"char",
		"const",
		"continue",
		"default",
		"do",
		"double",
		"else",
		"enum",
		"extern",
		"float",
		"for",
		"goto",
		"if",
		"int",
		"long",
		"register",
		"return",
		"short",
		"signed",
		"sizeof",
		"static",
		"struct",
		"switch",
		"typedef",
		"union",
		"unsigned",
		"void",
		"volatile",
		"while",
		"IDENTIFIER",

		//operators
		"+",
		"-",
		"++",
		"--",
		"+=",
		"-=",
		"/",
		"^",
		"&",
		"|",
		"&&",
		"||",
		">",
		"<",
		">=",
		"<=",
		"==",
		"?",

		//literal tokens
		"INTEGER_LITERAL",
		"CHAR_LITERAL",
		"FLOAT32_LITERAL",
		"FLOAT64_LITERAL",
		"STRING_LITERAL",

		//control tokens
		"\\n",
		"END"
	};
	return tok_names[type];
}