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

token michaelcc::preprocessor::scanner::scan_token()
{
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
}
