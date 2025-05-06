#include "preprocessor.hpp"

using namespace michaelcc;

char preprocessor::scanner::scan_char()
{
	if (index == m_source.size()) {
		return '\0';
	}

	char to_return = m_source.at(index);
	index++;
	
	if (to_return == '\n') {
		current_col = 1;
		current_row++;
	}
	else {
		current_col++;
	}

	return to_return;
}

char michaelcc::preprocessor::scanner::peek_char()
{
	if (index == m_source.size()) {
		return '\0';
	}

	return m_source.at(index);
}

bool michaelcc::preprocessor::scanner::scan_char_if_match(char match, bool in_loop)
{
	if (peek_char() == '\0' && in_loop) {
		throw panic("Unexpected EOF.");
	}

	if (peek_char() == match) {
		scan_char();
		return true;
	}
	return false;
}

char michaelcc::preprocessor::scanner::scan_char_literal()
{
	char c = scan_char();

	if (c == '\\') { //handle escape sequences here
		c = scan_char();

		switch (c)
		{
		case 'a':
			return (char)0x7;
		case 'b':
			return (char)0x8;
		case 'e':
			return (char)0x1B;
		case 'f':
			return (char)0xC;
		case 'n':
			return (char)0xA;
		case 'r':
			return (char)0xD;
		case 't':
			return (char)0x9;
		case 'v':
			return (char)0xB;
		case '\\':
			[[fallthrough]];
		case '\'':
			[[fallthrough]];
		case '\"':
			return c;
		default:
		{
			std::stringstream ss;
			ss << "Unrecognized escape sequence '\\" << c << "'.";
			throw panic(ss.str());
		}
		}
	}
}

token michaelcc::preprocessor::scanner::peek_token()
{
	if (!token_backlog.empty()) {
		return std::move(token_backlog.front());
	}

	token_backlog.emplace_back(scan_token());
	return std::move(token_backlog.front());
}

token michaelcc::preprocessor::scanner::scan_token()
{
	if (!token_backlog.empty()) {
		token to_return = std::move(token_backlog.front());
		token_backlog.pop_front();
		return to_return;
	}

	while (isspace(peek_char()) && peek_char() != '\n') {
		scan_char();
	}

	last_tok_begin = std::make_pair(current_row, current_col);
	if (isalpha(peek_char())) { //parse keyword/identifier
		std::string identifier;

		do {
			identifier.push_back(scan_char());
		} while (isalnum(peek_char()) || peek_char() == '_');

		auto it = keywords.find(identifier);
		if (it != keywords.end()) {
			return token(it->second, identifier, location().col());
		}

		return token(MICHAELCC_TOKEN_IDENTIFIER, identifier, location().col());
	}
	else if (isdigit(peek_char())) { //parse numerical literal
		std::string str_data;

		do {
			str_data.push_back(scan_char());
		} while (isdigit(peek_char()) || peek_char() == '.' || peek_char() == 'x' || (peek_char() >= 'A' && peek_char() <= 'F') || (peek_char() >= 'a' && peek_char() <= 'f'));

		/*
		* Rules for parsing numbers
		* 
		* If the number literal begins with 0x, parse as hex
		* If the number literal begins with 0b, parse as binary
		* If the number literal contains a period(.), parse as a double
		* If the number literal ends with a f parse as float
		* If the number literal ends with d parse as double
		*/

		try {
			if (str_data.find('.') != std::string::npos) {
				if (str_data.ends_with('f')) {
					str_data.pop_back();
					return token(std::stof(str_data), location().col());
				}
				else if (str_data.ends_with('d')) {
					str_data.pop_back();
					return token(std::stod(str_data), location().col());
				}
			}
			else if (str_data.starts_with("0x")) { //parse literal
				return token(MICHAELCC_TOKEN_INTEGER_LITERAL, std::stoull(str_data.substr(2), nullptr, 16));
			}
			else if (str_data.starts_with("0b")) {
				return token(MICHAELCC_TOKEN_INTEGER_LITERAL, std::stoull(str_data.substr(2), nullptr, 2));
			}
			else {
				return token(MICHAELCC_TOKEN_INTEGER_LITERAL, std::stoull(str_data));
			}
		}
		catch (const std::invalid_argument&) {
			std::stringstream ss;
			ss << "Invalid numerical literal \"" << str_data << "\".";
			throw panic(ss.str());
		}
		catch (const std::out_of_range&) {
			std::stringstream ss;
			ss << "Numerical literal is \"" << str_data << "\" too large to be represented in data unit.";
			throw panic(ss.str());
		}
	}
	else if (scan_char_if_match('#')) { //parse preprocessor directive
		std::string identifier;

		do {
			identifier.push_back(scan_char());
		} while (isalnum(peek_char()));

		auto it = preprocessor_keywords.find(identifier);
		if (it != preprocessor_keywords.end()) {
			return token(it->second, location().col());
		}

		std::stringstream ss;
		ss << "Invalid preprocessor directive #" << identifier << '.';
		throw panic(ss.str());
	}
	else if (scan_char_if_match('\"')) {
		std::string str;

		while (!scan_char_if_match('\"', true)) {
			str.push_back(scan_char_literal());
		}

		return token(MICHAELCC_TOKEN_STRING_LITERAL, str, location().col());
	}
	else if (scan_char_if_match('\'')) {
		char c = scan_char_literal();
		expect_char('\'');
		return token(MICHAELCC_TOKEN_CHAR_LITERAL, c);
	}
	else {
		char current = scan_char();

		switch (current)
		{
		case '[':
			return token(MICHAELCC_TOKEN_OPEN_BRACKET, location().col());
		case ']':
			return token(MICHAELCC_TOKEN_CLOSE_BRACKET, location().col());
		case '(':
			return token(MICHAELCC_TOKEN_OPEN_PAREN, location().col());
		case ')':
			return token(MICHAELCC_TOKEN_CLOSE_PAREN, location().col());
		case '{':
			return token(MICHAELCC_TOKEN_OPEN_BRACKET, location().col());
		case '}':
			return token(MICHAELCC_TOKEN_CLOSE_BRACKET, location().col());
		case ',':
			return token(MICHAELCC_TOKEN_COMMA, location().col());
		case ':':
			return token(MICHAELCC_TOKEN_COLON, location().col());
		case ';':
			return token(MICHAELCC_TOKEN_SEMICOLON, location().col());
		case '=':
			return token(scan_char_if_match('=') ? MICHAELCC_TOKEN_EQUALS : MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR, location().col());
		case '.':
			return token(MICHAELCC_TOKEN_PERIOD, location().col());
		case '~':
			return token(MICHAELCC_TOKEN_TILDE, location().col());
		case '+':
			if (scan_char_if_match('=')) {
				return token(MICHAELCC_TOKEN_INCREMENT_BY, location().col());
			}
			return token(scan_char_if_match('+') ? MICHAELCC_TOKEN_INCREMENT : MICHAELCC_TOKEN_PLUS, location().col());
		case '-':
			if (scan_char_if_match('=')) {
				return token(MICHAELCC_TOKEN_DECREMENT_BY, location().col());
			}
			else if (scan_char_if_match('>')) {
				return token(MICHAELCC_TOKEN_DEREFERENCE_GET, location().col());
			}
			return token(scan_char_if_match('+') ? MICHAELCC_TOKEN_INCREMENT : MICHAELCC_TOKEN_MINUS, location().col());
		case '/':
			if (scan_char_if_match('/')) { //single line comment
				//consume the rest of the line
				while(scan_char() != '\n') { }
				return token(location());
			}
			else if (scan_char_if_match('*')) { //multi-line comment
				for (;;) {
					if (scan_char() == '*' && scan_char() == '/') {
						break;
					}
				}
				return token(location());
			}
			return token(MICHAELCC_TOKEN_SLASH, location().col());
		case '^':
			return token(MICHAELCC_TOKEN_CARET, location().col());
		case '&':
			return token(scan_char_if_match('&') ? MICHAELCC_TOKEN_DOUBLE_AND : MICHAELCC_TOKEN_AND, location().col());
		case '|':
			return token(scan_char_if_match('|') ? MICHAELCC_TOKEN_DOUBLE_OR : MICHAELCC_TOKEN_OR, location().col());
		case '>':
			return token(scan_char_if_match('=') ? MICHAELCC_TOKEN_MORE_EQUAL : MICHAELCC_TOKEN_MORE, location().col());
		case '<':
			return token(scan_char_if_match('=') ? MICHAELCC_TOKEN_LESS_EQUAL : MICHAELCC_TOKEN_LESS, location().col());
		case '?':
			return token(MICHAELCC_TOKEN_QUESTION, location().col());
		case '\n':
			return token(MICHAELCC_TOKEN_NEWLINE, location().col());
		case '\0':
			return token(MICHAELCC_TOKEN_END, location().col());
		default:
		{
			std::stringstream ss;
			ss << "Unrecognized character \'" << current << '\'.';
			throw panic(ss.str());
		}
		}
	}
}

bool michaelcc::preprocessor::scanner::scan_token_if_match(token_type type)
{
	token token = peek_token();
	if (token.type() == type) {
		scan_token();
		return true;
	}
	return false;
}

void michaelcc::preprocessor::scanner::expect_char(char expected)
{
	char c = peek_char();
	if (c != expected) {
		std::stringstream ss;
		ss << "Expected char \'" << expected << "\' but got \'" << c << "\' instead.";
		throw panic(ss.str());
	}
	scan_char();
}

std::optional<std::filesystem::path> michaelcc::preprocessor::scanner::resolve_file_path(std::filesystem::path file_path)
{
	if (std::filesystem::exists(file_path)) {
		return file_path;
	}
	else if (std::filesystem::exists(m_file_name.parent_path() / file_path)) {
		return m_file_name.parent_path() / file_path;
	}
	return std::nullopt;
}
