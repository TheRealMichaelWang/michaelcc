#ifndef MICHAELCC_PREPROCESSOR_HPP
#define MICHAELCC_PREPROCESSOR_HPP

#include <string>
#include <vector>
#include <utility>
#include <map>
#include <deque>
#include <optional>
#include <filesystem>

#include "tokens.hpp"
#include "errors.hpp"

namespace michaelcc {
	class preprocessor {
	private:
		class scanner {
		private:
			const std::filesystem::path m_file_name;
			const std::string m_source;

			std::pair<size_t, size_t> last_tok_begin;
			token m_last_tok = token(MICHAELCC_TOKEN_END, 0);

			size_t index = 0;
			size_t current_row = 1;
			size_t current_col = 1;

			std::deque<token> token_backlog;

			const std::map<std::string, token_type> preprocessor_keywords{
				{ "define", MICHAELCC_PREPROCESSOR_TOKEN_DEFINE },
				{ "ifdef", MICHAELCC_PREPROCESSOR_TOKEN_IFDEF },
				{ "ifndef", MICHAELCC_PREPROCESSOR_TOKEN_IFNDEF },
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
			bool scan_char_if_match(char match, bool in_loop=false);

			char scan_char_literal();

			const compilation_error panic(const std::string msg) const noexcept {
				return compilation_error(msg, source_location(current_row, current_col, m_file_name));
			}
		public:
			scanner(const std::string m_source, const std::filesystem::path m_file_name) : m_source(m_source), m_file_name(m_file_name), last_tok_begin(1,1) {

			}

			const source_location location() const noexcept {
				return source_location(last_tok_begin.first, last_tok_begin.second, m_file_name);
			}

			const source_location end_location() const noexcept {
				return source_location(current_row, current_col + 1, m_file_name);
			}

			const std::filesystem::path file_name() const noexcept {
				return m_file_name;
			}

			token peek_token(bool in_macro_definition = false);
			token scan_token(bool in_macro_definition =false);

			bool scan_token_if_match(token_type type, bool in_while_loop = false, bool in_macro_definition = false);

			void push_backlog(const token token) {
				token_backlog.push_back(token);
			}

			void push_backlog_macro(std::vector<token>&& macro_expansion) {
				while (!macro_expansion.empty())
				{
					token_backlog.emplace_front(std::move(macro_expansion.back()));
					macro_expansion.pop_back();
				}
			}

			void expect_char(char expected);

			std::optional<std::filesystem::path> resolve_file_path(std::filesystem::path file_path);
		};

		class definition {
		private:
			const std::string m_name;
			const std::vector<std::string> m_params;

			const std::vector<token> m_tokens;
			const std::optional<source_location> m_location;
		public:
			definition(const std::string name, const std::vector<std::string> params, const std::vector<token> tokens, std::optional<source_location> location) : m_name(name), m_params(params), m_tokens(tokens), m_location(location) {
				
			}

			void expand(scanner& output, std::vector<std::vector<token>> arguments, const preprocessor& preprocessor) const;

			const source_location location() {
				return m_location.value();
			}
		};

		struct preprocessor_scope {
			token_type type;
			source_location begin_location;
			bool skip;
			bool override_skip;

			preprocessor_scope(token_type type, source_location begin_location, bool skip, bool override_skip) : type(type), begin_location(begin_location), skip(skip), override_skip(override_skip) {

			}
		};

		std::vector<token> m_result;
		std::vector<scanner> m_scanners;
		std::map<std::string, definition> m_definitions;

		const compilation_error panic(const std::string msg) const noexcept {
			return compilation_error(msg, m_scanners.back().location());
		}

		token expect_token(token_type type);
	public:
		preprocessor(std::string source, std::string file_name) {
			m_scanners.emplace_back(scanner(source, file_name));
		}

		void preprocess();

		const std::vector<token>& result() const noexcept {
			return m_result;
		}
	};
}

#endif