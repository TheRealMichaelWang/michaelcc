#include <fstream>
#include "preprocessor.hpp"

using namespace michaelcc;

void preprocessor::definition::expand(scanner& output, std::vector<std::vector<token>> arguments, const preprocessor& preprocessor) const
{
	if (arguments.size() != m_params.size()) {
		std::stringstream ss;
		ss << "Macro definition " << m_name << " expected " << m_params.size() << " argument(s), but got " << arguments.size() << " argument(s) instead.";
		throw preprocessor.panic(ss.str());
	}

	std::map<std::string, size_t> param_offsets;
	for (size_t i = 0; i < m_params.size(); i++) {
		param_offsets.insert({ m_params.at(i), i });
	}

	for (const token& token : m_tokens) {
		if (token.type() == MICHAELCC_TOKEN_IDENTIFIER) {
			auto it = param_offsets.find(token.string());
			if (it != param_offsets.end()) {
				auto& arg = arguments.at(it->second);
				for (auto& token : arg) {
					output.push_backlog(token);
				}
				continue;
			}
		}
		output.push_backlog(token);
	}
}

token michaelcc::preprocessor::expect_token(token_type type)
{
	token tok = m_scanners.back().scan_token();
	if (tok.type() != type) {
		std::stringstream ss;
		ss << "Expected " << token_to_str(type) << " but got " << token_to_str(tok.type()) << " instead.";
		throw panic(ss.str());
	}
	return tok;
}

void preprocessor::preprocess()
{
	std::vector<preprocessor_scope> preprocessor_scopes;
	m_result.push_back(token(m_scanners.front().location()));

	while (!m_scanners.empty()) {
		auto& scanner = m_scanners.back();
		
		source_location location = scanner.location();
		token tok = scanner.scan_token();

		if (!preprocessor_scopes.empty() && preprocessor_scopes.back().skip) {
			if (tok.is_preprocessor_condition()) {
				preprocessor_scopes.push_back(preprocessor_scope(tok.type(), location, true, true));
			}
			else if (tok.type() == MICHAELCC_PREPROCESSOR_TOKEN_ELSE && !preprocessor_scopes.back().override_skip) {
				preprocessor_scopes.pop_back();
				preprocessor_scopes.push_back(preprocessor_scope(tok.type(), location, false, false));
				m_result.push_back(token(location));
			}
			else if (tok.type() == MICHAELCC_PREPROCESSOR_TOKEN_ENDIF) {
				preprocessor_scopes.pop_back();
				m_result.push_back(token(location));
			}
			continue;
		}

		switch (tok.type())
		{
		case MICHAELCC_PREPROCESSOR_TOKEN_INCLUDE: {
			std::string requested_path = expect_token(MICHAELCC_TOKEN_STRING_LITERAL).string();
			auto file_path = scanner.resolve_file_path(requested_path);
			if (!file_path.has_value()) {
				std::stringstream ss;
				ss << "File \"" << requested_path << "\" doesn't exist.";
				throw panic(ss.str());
			}

			std::ifstream infile(file_path.value());

			std::stringstream ss;
			ss << infile.rdbuf();

			if (!infile.good()) {
				std::stringstream ss;
				ss << "Unable to open file \"" << file_path.value() << "\".";
				throw panic(ss.str());
			}


			m_scanners.push_back(preprocessor::scanner(ss.str(), file_path.value()));
			m_result.push_back(token(m_scanners.back().location()));
			continue;
		}
		case MICHAELCC_PREPROCESSOR_TOKEN_DEFINE: {
			std::string macro_name = expect_token(MICHAELCC_TOKEN_IDENTIFIER).string();
			{
				auto it = m_definitions.find(macro_name);
				if (it != m_definitions.end()) {
					std::stringstream ss;
					ss << "Redefinition of macro " << macro_name << " declared at " << it->second.location().to_string() << '.';
					throw panic(ss.str());
				}
			}

			std::vector<std::string> params;
			if (scanner.scan_token_if_match(MICHAELCC_TOKEN_OPEN_PAREN)) {
				if (scanner.scan_token_if_match(MICHAELCC_TOKEN_CLOSE_PAREN)) {
					return;
				}

				do {
					params.push_back(expect_token(MICHAELCC_TOKEN_IDENTIFIER).string());
				} while (scanner.scan_token_if_match(MICHAELCC_TOKEN_COMMA));

				expect_token(MICHAELCC_TOKEN_CLOSE_PAREN);
			}

			std::vector<token> tokens;
			while (!scanner.scan_token_if_match(MICHAELCC_TOKEN_NEWLINE)) {
				token tok = scanner.scan_token();
				if (tok.is_preprocessor()) {
					std::stringstream ss;
					ss << "Unexpected preprocessor token " << token_to_str(tok.type()) << '.';
					throw panic(ss.str());
				}
				tokens.push_back(tok);
			}

			m_definitions.insert({ macro_name, definition(macro_name, params, tokens, location) });

			break;
		}
		case MICHAELCC_PREPROCESSOR_TOKEN_IFNDEF:
			[[fallthrough]];
		case MICHAELCC_PREPROCESSOR_TOKEN_IFDEF: {
			std::string identifier = expect_token(MICHAELCC_TOKEN_IDENTIFIER).string();

			bool condition = ((tok.type() == MICHAELCC_PREPROCESSOR_TOKEN_IFDEF) ^ m_definitions.contains(identifier));

			preprocessor_scopes.push_back(preprocessor_scope(tok.type(), location, condition, false));

			break;
		}
		case MICHAELCC_PREPROCESSOR_TOKEN_ELSE:
			if (preprocessor_scopes.empty()) {
				throw panic("Unexpected preprocessor else. No matching #ifdef or #ifndef.");
			}
			if (preprocessor_scopes.back().type == MICHAELCC_PREPROCESSOR_TOKEN_ELSE) {
				std::stringstream ss;
				ss << "Unexpected preprocessor else. Found previous #else at " << preprocessor_scopes.back().begin_location.to_string() << '.';
				throw panic(ss.str());
			}
			preprocessor_scopes.pop_back();
			preprocessor_scopes.push_back(preprocessor_scope(tok.type(), location, true, false));
			break;
		case MICHAELCC_PREPROCESSOR_TOKEN_ENDIF:
			if (preprocessor_scopes.empty()) {
				throw panic("Unexpected preprocessor end. No matching #ifdef or #ifndef.");
			}
			preprocessor_scopes.pop_back();
			break;
		case MICHAELCC_TOKEN_END: {
			if (!preprocessor_scopes.empty()) {
				std::stringstream ss;
				for (const auto& preprocessor_scope : preprocessor_scopes) {
					ss << "Preprocessor directive " << token_to_str(preprocessor_scope.type) << " declared at " << preprocessor_scope.begin_location.to_string() << " expected end but got none.";
					if (&preprocessor_scope != &preprocessor_scopes.back()) {
						ss << '\n';
					}
				}
				throw panic(ss.str());
			}

			m_scanners.pop_back();
			continue;
		}
		case MICHAELCC_TOKEN_IDENTIFIER:
		{
			auto it = m_definitions.find(tok.string());
			if (it != m_definitions.end()) {
				std::vector<std::vector<token>> arguments;
				if (scanner.scan_token_if_match(MICHAELCC_TOKEN_OPEN_PAREN)) {
					do {
						std::vector<token> argument;

						token_type tok_type;
						do {
							token tok2 = scanner.scan_token();
							argument.push_back(tok2);
							tok_type = tok2.type();
						} while (tok_type != MICHAELCC_TOKEN_COMMA && tok_type != MICHAELCC_TOKEN_CLOSE_PAREN);
						argument.pop_back();
						arguments.emplace_back(std::move(argument));
					} while (tok.type() != MICHAELCC_TOKEN_CLOSE_PAREN);
				}
				it->second.expand(scanner, arguments, *this);
				m_result.push_back(token(scanner.location()));
				continue;
			}
		}
		[[fallthrough]];
		default:
			m_result.push_back(tok);
			break;
		}
	}
}
