#ifndef MICHAELCC_PREPROCESSOR_HPP
#define MICHAELCC_PREPROCESSOR_HPP

#include "tokens.hpp"
#include <vector>
#include <map>
#include <deque>
#include <optional>
#include <filesystem>

namespace michaelcc {

class preprocessor {
public:
	preprocessor(std::string source, std::filesystem::path file);
	void run();
	std::vector<token> take_result() { return std::move(m_result); }

private:
	class scanner {
	public:
		scanner(std::string source, std::filesystem::path file);

		source_location loc() const;
		const std::filesystem::path& file() const noexcept { return m_file; }

		token scan(bool in_macro = false);
		const token& peek(bool in_macro = false);
		bool match(token_type type, bool in_macro = false);
		void inject(std::vector<token> tokens);

		std::optional<std::filesystem::path> resolve(const std::filesystem::path& p);

	private:
		std::string m_src;
		std::filesystem::path m_file;
		size_t m_pos = 0;
		size_t m_row = 1, m_col = 1;
		std::deque<token> m_buf;

		char peek_char() const;
		char get_char();
		bool match_char(char c);
		char escape();
		[[noreturn]] void error(const std::string& msg);

		static const std::map<std::string, token_type> KEYWORDS;
		static const std::map<std::string, token_type> PP_KEYWORDS;
	};

	struct macro {
		std::vector<std::string> params;
		std::vector<token> body;
		source_location loc;
	};

	struct scope {
		token_type type;
		source_location loc;
		bool skip, locked;
	};

	std::vector<token> m_result;
	std::vector<scanner> m_scanners;
	std::map<std::string, macro> m_macros;

	void expand_macro(const std::string& name, const macro& m);
	[[noreturn]] void error(const std::string& msg);
	token expect(token_type type);
};

}

#endif
