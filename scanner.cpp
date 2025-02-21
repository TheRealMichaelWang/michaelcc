#include "preprocessor.hpp"

using namespace michaelcc;

char preprocessor::scanner::scan_char()
{
	if (index == m_source.size()) {
		return '0';
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
		return '0';
	}

	return m_source.at(index);
}

bool michaelcc::preprocessor::scanner::scan_char_if_match(char match)
{
	if (peek_char() == match) {
		scan_char();
		return true;
	}
	return false;
}

token michaelcc::preprocessor::scanner::peek_token()
{
	if (!token_backlog.empty()) {
		return token_backlog.front();
	}

	token_backlog.emplace_back(scan_token());
	return token_backlog.front();
}

token michaelcc::preprocessor::scanner::scan_token()
{
	if (!token_backlog.empty()) {
		token to_return = token_backlog.front();
		token_backlog.pop_front();
		return to_return;
	}

	while (isspace(peek_char())) {
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
			return token(it->second, identifier);
		}

		return token(MICHAELCC_TOKEN_IDENTIFIER, identifier);
	}
	else if (isdigit(peek_char())) { //parse numerical literal
		std::string str_data;

		do {
			str_data.push_back(scan_char());
		} while (isdigit(peek_char()) || peek_char() == '.' || peek_char() == 'x');

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
					return token(std::stof(str_data));
				}
				else if (str_data.ends_with('d')) {
					str_data.pop_back();
					return token(std::stod(str_data));
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
	else if (peek_char() == '#') { //parse preprocessor directive
		scan_char();
		std::string identifier;

		do {
			identifier.push_back(scan_char());
		} while (isalnum(peek_char()));

		auto it = preprocessor_keywords.find(identifier);
		if (it != preprocessor_keywords.end()) {
			return token(it->second);
		}

		std::stringstream ss;
		ss << "Invalid preprocessor directive #" << identifier << '.';
		throw panic(ss.str());
	}
	else {
		char current = scan_char();

		switch (current)
		{
		case '[':
			return token(MICHAELCC_TOKEN_OPEN_BRACKET);
		case ']':
			return token(MICHAELCC_TOKEN_CLOSE_BRACKET);
		case '(':
			return token(MICHAELCC_TOKEN_OPEN_PAREN);
		case ')':
			return token(MICHAELCC_TOKEN_CLOSE_PAREN);
		case '{':
			return token(MICHAELCC_TOKEN_OPEN_BRACKET);
		case '}':
			return token(MICHAELCC_TOKEN_CLOSE_BRACKET);
		case ',':
			return token(MICHAELCC_TOKEN_COMMA);
		case ':':
			return token(MICHAELCC_TOKEN_COLON);
		case ';':
			return token(MICHAELCC_TOKEN_SEMICOLON);
		case '=':
			return token(scan_char_if_match('=') ? MICHAELCC_TOKEN_EQUALS : MICHAELCC_TOKEN_ASSIGNMENT_OPERATOR);
		case '.':
			return token(MICHAELCC_TOKEN_PERIOD);
		case '~':
			return token(MICHAELCC_TOKEN_TILDE);
		case '+':
			if (scan_char_if_match('=')) {
				return token(MICHAELCC_TOKEN_INCREMENT_BY);
			}
			return token(scan_char_if_match('+') ? MICHAELCC_TOKEN_INCREMENT : MICHAELCC_TOKEN_PLUS);
		case '-':
			if (scan_char_if_match('=')) {
				return token(MICHAELCC_TOKEN_DECREMENT_BY);
			}
			return token(scan_char_if_match('+') ? MICHAELCC_TOKEN_INCREMENT : MICHAELCC_TOKEN_MINUS);
		case '/':
			if (scan_char_if_match('/')) { //single line comment
				//consume the rest of the line
				while(scan_char() != '\n') { }
				return scan_token();
			}
			else if (scan_char_if_match('*')) { //multi-line comment
				for (;;) {
					if (scan_char() == '*' && scan_char() == '/') {
						break;
					}
				}
				return scan_token();
			}
			return token(MICHAELCC_TOKEN_SLASH);
		case '^':
			return token(MICHAELCC_TOKEN_CARET);
		case '&':
			return token(scan_char_if_match('&') ? MICHAELCC_TOKEN_DOUBLE_AND : MICHAELCC_TOKEN_AND);
		case '|':
			return token(scan_char_if_match('|') ? MICHAELCC_TOKEN_DOUBLE_OR : MICHAELCC_TOKEN_OR);
		case '>':
			return token(scan_char_if_match('=') ? MICHAELCC_TOKEN_MORE_EQUAL : MICHAELCC_TOKEN_MORE);
		case '<':
			return token(scan_char_if_match('=') ? MICHAELCC_TOKEN_LESS_EQUAL : MICHAELCC_TOKEN_LESS);
		case '?':
			return token(MICHAELCC_TOKEN_QUESTION);
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
