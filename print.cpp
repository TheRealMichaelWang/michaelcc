#include "tokens.hpp"

namespace michaelcc {

static const char* const TOKEN_NAMES[] = {
	"#define", "#ifdef", "#ifndef", "#else", "#endif", "#include", "#line", "#",
	"[", "]", "(", ")", "{", "}", ",", ":", ";", "=", ".", "~", "->",
	"auto", "break", "case", "char", "const", "continue", "default", "do",
	"double", "else", "enum", "extern", "float", "for", "goto", "if",
	"int", "long", "register", "return", "short", "signed", "sizeof", "static",
	"struct", "switch", "typedef", "union", "unsigned", "void", "volatile",
	"restrict", "while", "<identifier>",
	"+", "-", "*", "++", "--", "+=", "-=", "/", "^", "&", "|", "&&", "||",
	">", "<", ">=", "<=", "==", "?", "%", "!", "!=", "<<", ">>",
	"*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=",
	"<int>", "<char>", "<float>", "<double>", "<string>",
	"\\n", "EOF"
};

std::string token_type_str(token_type type) {
	return TOKEN_NAMES[type];
}

std::string token_str(const token& tok) {
	switch (tok.type()) {
	case TOKEN_IDENTIFIER:
	case TOKEN_PP_STRINGIFY:
		return tok.str();
	case TOKEN_STRING_LITERAL:
		return "\"" + tok.str() + "\"";
	default:
		return token_type_str(tok.type());
	}
}

}
