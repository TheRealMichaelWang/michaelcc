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
	}
	return tok;
}

void preprocessor::preprocess()
{
	while (!m_scanners.empty()) {
		auto& scanner = m_scanners.back();

		token tok = scanner.scan_token();

		switch (tok.type())
		{
		case token_type::MICHAELCC_TOKEN_IDENTIFIER:
		{
			auto it = m_definitions.find(tok.string());
			if (it != m_definitions.end()) {
				std::vector<std::vector<token>> arguments;
				if (scanner.scan_token_if_match(MICHAELCC_TOKEN_OPEN_PAREN)) {
					do {
						std::vector<token> argument;

						do {
							tok = scanner.scan_token();
							argument.push_back(tok);
						} while (tok.type() != MICHAELCC_TOKEN_COMMA && tok.type() != MICHAELCC_TOKEN_CLOSE_PAREN);
						argument.pop_back();
						arguments.emplace_back(std::move(argument));
					} while (tok.type() == MICHAELCC_TOKEN_CLOSE_PAREN);
				}
				it->second.expand(scanner, arguments, *this);
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
