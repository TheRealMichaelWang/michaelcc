#pragma once

#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <map>

#include "tokens.hpp"
#include "errors.hpp"

namespace michaelcc {
	class preprocessor {
	private:
		class scanner {
		private:
			const std::string m_file_name;
			const std::string m_source;

			std::pair<size_t, size_t> last_tok_begin;
			token m_last_tok = token(MICHAELCC_TOKEN_END);

			size_t index = 0;
			size_t current_row = 1;
			size_t current_col = 1;

			const std::map<std::string, token_type> preprocessor_keywords{
				{ "define", MICHAELCC_PREPROCESSOR_TOKEN_DEFINE },
				{ "ifdef", MICHAELCC_PREPROCESSOR_TOKEN_IFDEF },
				{ "else", MICHAELCC_PREPROCESSOR_TOKEN_ELSE },
				{ "endif", MICHAELCC_PREPROCESSOR_TOKEN_ENDIF },
				{ "include", MICHAELCC_PREPROCESSOR_TOKEN_INCLUDE }
			};

			const std::map<std::string, token_type> keywords = {
				{ "auto", MICHAELCC_TOKEN_AUTO },
				{ "break", MICHAELCC_TOKEN_BREAK },
				{ "case", MICHAELCC_TOKEN_CASE },
				{ "char", MICHAELCC_TOKEN_CHAR },
				{ "const", MICHAELCC_TOKEN_CONST },
				{ "continue", MICHAELCC_TOKEN_CONTINUE },
				{ "default", MICHAELCC_TOKEN_DEFAULT },
				{ "do", MICHAELCC_TOKEN_DO },
				{ "double", MICHAELCC_TOKEN_DOUBLE },
				{ "else", MICHAELCC_TOKEN_ELSE },
				{ "enum", MICHAELCC_TOKEN_ENUM },
				{ "extern", MICHAELCC_TOKEN_EXTERN },
				{ "float", MICHAELCC_TOKEN_FLOAT },
				{ "for", MICHAELCC_TOKEN_FOR },
				{ "goto", MICHAELCC_TOKEN_GOTO },
				{ "if", MICHAELCC_TOKEN_IF },
				{ "int", MICHAELCC_TOKEN_INT },
				{ "long", MICHAELCC_TOKEN_LONG },
				{ "register", MICHAELCC_TOKEN_REGISTER },
				{ "return", MICHAELCC_TOKEN_RETURN },
				{ "short", MICHAELCC_TOKEN_SHORT },
				{ "signed", MICHAELCC_TOKEN_SIGNED },
				{ "sizeof", MICHAELCC_TOKEN_SIZEOF },
				{ "static", MICHAELCC_TOKEN_STATIC },
				{ "struct", MICHAELCC_TOKEN_STRUCT },
				{ "switch", MICHAELCC_TOKEN_SWITCH },
				{ "typedef", MICHAELCC_TOKEN_TYPEDEF },
				{ "union", MICHAELCC_TOKEN_UNION },
				{ "unsigned", MICHAELCC_TOKEN_UNSIGNED },
				{ "void", MICHAELCC_TOKEN_VOID },
				{ "volatile", MICHAELCC_TOKEN_VOLATILE },
				{ "while", MICHAELCC_TOKEN_WHILE },
			};

			char scan_char();
			char peek_char();
			bool scan_if_match(char match);
		public:
			scanner(const std::string m_source, const std::string m_file_name) : m_source(m_source), m_file_name(m_file_name), last_tok_begin(1,1) {

			}

			token scan_token();

			const compilation_error panic(const std::string msg) const noexcept {
				return compilation_error(msg, current_row, current_col, m_file_name);
			}
		};

		std::vector<token> m_result;
		std::vector<scanner> m_scanners;

	public:
		preprocessor(std::string source, std::string file_name) {
			m_scanners.push_back(scanner(source, file_name));
		}
	};
}