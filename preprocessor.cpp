#include "preprocessor.hpp"
#include <fstream>
#include <sstream>
#include <cctype>

namespace michaelcc {

const std::map<std::string, token_type> preprocessor::scanner::KEYWORDS = {
	{"auto", TOKEN_AUTO}, {"break", TOKEN_BREAK}, {"case", TOKEN_CASE},
	{"char", TOKEN_CHAR}, {"const", TOKEN_CONST}, {"continue", TOKEN_CONTINUE},
	{"default", TOKEN_DEFAULT}, {"do", TOKEN_DO}, {"double", TOKEN_DOUBLE},
	{"else", TOKEN_ELSE}, {"enum", TOKEN_ENUM}, {"extern", TOKEN_EXTERN},
	{"float", TOKEN_FLOAT}, {"for", TOKEN_FOR}, {"goto", TOKEN_GOTO},
	{"if", TOKEN_IF}, {"int", TOKEN_INT}, {"long", TOKEN_LONG},
	{"register", TOKEN_REGISTER}, {"return", TOKEN_RETURN}, {"short", TOKEN_SHORT},
	{"signed", TOKEN_SIGNED}, {"sizeof", TOKEN_SIZEOF}, {"static", TOKEN_STATIC},
	{"struct", TOKEN_STRUCT}, {"switch", TOKEN_SWITCH}, {"typedef", TOKEN_TYPEDEF},
	{"union", TOKEN_UNION}, {"unsigned", TOKEN_UNSIGNED}, {"void", TOKEN_VOID},
	{"volatile", TOKEN_VOLATILE}, {"restrict", TOKEN_RESTRICT}, {"while", TOKEN_WHILE}
};

const std::map<std::string, token_type> preprocessor::scanner::PP_KEYWORDS = {
	{"define", TOKEN_PP_DEFINE}, {"ifdef", TOKEN_PP_IFDEF}, {"ifndef", TOKEN_PP_IFNDEF},
	{"else", TOKEN_PP_ELSE}, {"endif", TOKEN_PP_ENDIF}, {"include", TOKEN_PP_INCLUDE}
};

preprocessor::scanner::scanner(std::string source, std::filesystem::path file)
	: m_src(std::move(source)), m_file(std::move(file)) {}

source_location preprocessor::scanner::loc() const {
	return source_location(m_row, m_col, m_file);
}

char preprocessor::scanner::peek_char() const {
	return m_pos < m_src.size() ? m_src[m_pos] : '\0';
}

char preprocessor::scanner::get_char() {
	if (m_pos >= m_src.size()) return '\0';
	char c = m_src[m_pos++];
	if (c == '\n') { ++m_row; m_col = 1; }
	else { ++m_col; }
	return c;
}

bool preprocessor::scanner::match_char(char c) {
	if (peek_char() == c) { get_char(); return true; }
	return false;
}

void preprocessor::scanner::error(const std::string& msg) {
	throw compile_error(msg, loc());
}

char preprocessor::scanner::escape() {
	char c = get_char();
	switch (c) {
	case 'a': return '\a'; case 'b': return '\b'; case 'e': return '\x1b';
	case 'f': return '\f'; case 'n': return '\n'; case 'r': return '\r';
	case 't': return '\t'; case 'v': return '\v'; case '0': return '\0';
	case '\\': case '\'': case '"': return c;
	case 'x': {
		std::string hex;
		for (int i = 0; i < 2 && std::isxdigit(peek_char()); ++i)
			hex += get_char();
		return static_cast<char>(std::stoi(hex, nullptr, 16));
	}
	default: error("invalid escape sequence");
	}
}

const token& preprocessor::scanner::peek(bool in_macro) {
	if (m_buf.empty()) m_buf.push_back(scan(in_macro));
	return m_buf.front();
}

bool preprocessor::scanner::match(token_type type, bool in_macro) {
	if (peek(in_macro).type() == type) {
		if (!m_buf.empty()) m_buf.pop_front();
		return true;
	}
	return false;
}

void preprocessor::scanner::inject(std::vector<token> tokens) {
	for (auto it = tokens.rbegin(); it != tokens.rend(); ++it)
		m_buf.push_front(std::move(*it));
}

std::optional<std::filesystem::path> preprocessor::scanner::resolve(const std::filesystem::path& p) {
	if (std::filesystem::exists(p)) return p;
	auto rel = m_file.parent_path() / p;
	if (std::filesystem::exists(rel)) return rel;
	return std::nullopt;
}

token preprocessor::scanner::scan(bool in_macro) {
	if (!m_buf.empty()) {
		token t = std::move(m_buf.front());
		m_buf.pop_front();
		return t;
	}

	while (std::isspace(peek_char()) && peek_char() != '\n') get_char();

	size_t start_col = m_col;

	if (std::isalpha(peek_char()) || peek_char() == '_') {
		std::string id;
		while (std::isalnum(peek_char()) || peek_char() == '_') id += get_char();
		auto it = KEYWORDS.find(id);
		if (it != KEYWORDS.end()) return token(it->second, id, start_col);
		return token(TOKEN_IDENTIFIER, std::move(id), start_col);
	}

	if (std::isdigit(peek_char())) {
		std::string num;
		while (std::isdigit(peek_char()) || peek_char() == '.' ||
			   peek_char() == 'x' || peek_char() == 'X' ||
			   (num.find('x') != std::string::npos && std::isxdigit(peek_char())))
			num += get_char();

		bool is_float = num.find('.') != std::string::npos;
		if (match_char('f') || match_char('F')) {
			return token(TOKEN_FLOAT_LITERAL, std::stod(num), start_col);
		}
		if (is_float) {
			return token(TOKEN_DOUBLE_LITERAL, std::stod(num), start_col);
		}
		uint64_t val = (num.starts_with("0x") || num.starts_with("0X"))
			? std::stoull(num.substr(2), nullptr, 16)
			: std::stoull(num);
		return token(TOKEN_INT_LITERAL, val, start_col);
	}

	if (match_char('#')) {
		std::string id;
		while (std::isalpha(peek_char())) id += get_char();
		auto it = PP_KEYWORDS.find(id);
		if (it != PP_KEYWORDS.end()) return token(it->second, start_col);
		if (in_macro) return token(TOKEN_PP_STRINGIFY, std::move(id), start_col);
		error("unknown preprocessor directive #" + id);
	}

	if (match_char('"')) {
		std::string s;
		while (peek_char() != '"' && peek_char() != '\0') {
			s += peek_char() == '\\' ? (get_char(), escape()) : get_char();
		}
		if (!match_char('"')) error("unterminated string");
		return token(TOKEN_STRING_LITERAL, std::move(s), start_col);
	}

	if (match_char('\'')) {
		char c = peek_char() == '\\' ? (get_char(), escape()) : get_char();
		if (!match_char('\'')) error("unterminated char literal");
		return token(TOKEN_CHAR_LITERAL, static_cast<uint64_t>(c), start_col);
	}

	char c = get_char();
	switch (c) {
	case '[': return token(TOKEN_LBRACKET, start_col);
	case ']': return token(TOKEN_RBRACKET, start_col);
	case '(': return token(TOKEN_LPAREN, start_col);
	case ')': return token(TOKEN_RPAREN, start_col);
	case '{': return token(TOKEN_LBRACE, start_col);
	case '}': return token(TOKEN_RBRACE, start_col);
	case ',': return token(TOKEN_COMMA, start_col);
	case ':': return token(TOKEN_COLON, start_col);
	case ';': return token(TOKEN_SEMICOLON, start_col);
	case '.': return token(TOKEN_DOT, start_col);
	case '~': return token(TOKEN_TILDE, start_col);
	case '?': return token(TOKEN_QUESTION, start_col);

	case '+': return token(match_char('=') ? TOKEN_PLUS_ASSIGN : match_char('+') ? TOKEN_INC : TOKEN_PLUS, start_col);
	case '-':
		if (match_char('=')) return token(TOKEN_MINUS_ASSIGN, start_col);
		if (match_char('-')) return token(TOKEN_DEC, start_col);
		if (match_char('>')) return token(TOKEN_ARROW, start_col);
		return token(TOKEN_MINUS, start_col);
	case '*': return token(match_char('=') ? TOKEN_STAR_ASSIGN : TOKEN_STAR, start_col);
	case '/':
		if (match_char('/')) { while (peek_char() && peek_char() != '\n') get_char(); return scan(in_macro); }
		if (match_char('*')) {
			while (!(get_char() == '*' && match_char('/'))) if (peek_char() == '\0') error("unterminated comment");
			return scan(in_macro);
		}
		return token(match_char('=') ? TOKEN_SLASH_ASSIGN : TOKEN_SLASH, start_col);
	case '%': return token(match_char('=') ? TOKEN_PERCENT_ASSIGN : TOKEN_PERCENT, start_col);
	case '=': return token(match_char('=') ? TOKEN_EQ : TOKEN_ASSIGN, start_col);
	case '!': return token(match_char('=') ? TOKEN_NE : TOKEN_BANG, start_col);
	case '<':
		if (match_char('=')) return token(TOKEN_LE, start_col);
		if (match_char('<')) return token(match_char('=') ? TOKEN_LSHIFT_ASSIGN : TOKEN_LSHIFT, start_col);
		return token(TOKEN_LT, start_col);
	case '>':
		if (match_char('=')) return token(TOKEN_GE, start_col);
		if (match_char('>')) return token(match_char('=') ? TOKEN_RSHIFT_ASSIGN : TOKEN_RSHIFT, start_col);
		return token(TOKEN_GT, start_col);
	case '&': return token(match_char('=') ? TOKEN_AMP_ASSIGN : match_char('&') ? TOKEN_AMP_AMP : TOKEN_AMP, start_col);
	case '|': return token(match_char('=') ? TOKEN_PIPE_ASSIGN : match_char('|') ? TOKEN_PIPE_PIPE : TOKEN_PIPE, start_col);
	case '^': return token(match_char('=') ? TOKEN_CARET_ASSIGN : TOKEN_CARET, start_col);
	case '\n': return token(TOKEN_NEWLINE, start_col);
	case '\0': return token(TOKEN_END, start_col);
	default: error(std::string("unexpected character '") + c + "'");
	}
}

preprocessor::preprocessor(std::string source, std::filesystem::path file) {
	m_scanners.emplace_back(std::move(source), std::move(file));
}

void preprocessor::error(const std::string& msg) {
	throw compile_error(msg, m_scanners.back().loc());
}

token preprocessor::expect(token_type type) {
	auto tok = m_scanners.back().scan();
	if (tok.type() != type) error("expected " + token_type_str(type));
	return tok;
}

void preprocessor::run() {
	std::vector<scope> scopes;
	m_result.emplace_back(m_scanners.front().loc());

	while (!m_scanners.empty()) {
		auto& sc = m_scanners.back();
		token tok = sc.scan();

		if (!scopes.empty() && scopes.back().skip) {
			if (tok.is_pp_cond()) {
				scopes.push_back({tok.type(), sc.loc(), true, true});
			} else if (tok.type() == TOKEN_PP_ELSE && !scopes.back().locked) {
				scopes.back() = {TOKEN_PP_ELSE, sc.loc(), false, false};
				m_result.emplace_back(sc.loc());
			} else if (tok.type() == TOKEN_PP_ENDIF) {
				scopes.pop_back();
				m_result.emplace_back(sc.loc());
			}
			continue;
		}

		switch (tok.type()) {
		case TOKEN_PP_INCLUDE: {
			auto path = expect(TOKEN_STRING_LITERAL).str();
			auto resolved = sc.resolve(path);
			if (!resolved) error("file not found: " + path);
			std::ifstream f(*resolved);
			std::ostringstream ss; ss << f.rdbuf();
			m_scanners.emplace_back(ss.str(), *resolved);
			m_result.emplace_back(m_scanners.back().loc());
			break;
		}
		case TOKEN_PP_DEFINE: {
			auto name = expect(TOKEN_IDENTIFIER).str();
			if (m_macros.contains(name)) error("redefinition of macro " + name);
			macro m;
			m.loc = sc.loc();
			if (sc.match(TOKEN_LPAREN)) {
				if (!sc.match(TOKEN_RPAREN)) {
					do { m.params.push_back(expect(TOKEN_IDENTIFIER).str()); }
					while (sc.match(TOKEN_COMMA));
					expect(TOKEN_RPAREN);
				}
			}
			while (!sc.match(TOKEN_NEWLINE, true)) m.body.push_back(sc.scan(true));
			m_macros[name] = std::move(m);
			m_result.emplace_back(sc.loc());
			break;
		}
		case TOKEN_PP_IFDEF:
		case TOKEN_PP_IFNDEF: {
			auto id = expect(TOKEN_IDENTIFIER).str();
			bool exists = m_macros.contains(id);
			bool skip = (tok.type() == TOKEN_PP_IFDEF) != exists;
			scopes.push_back({tok.type(), sc.loc(), skip, false});
			break;
		}
		case TOKEN_PP_ELSE:
			if (scopes.empty()) error("unexpected #else");
			if (scopes.back().type == TOKEN_PP_ELSE) error("duplicate #else");
			scopes.back() = {TOKEN_PP_ELSE, sc.loc(), true, false};
			break;
		case TOKEN_PP_ENDIF:
			if (scopes.empty()) error("unexpected #endif");
			scopes.pop_back();
			break;
		case TOKEN_END:
			if (!scopes.empty()) error("unterminated #if/#ifdef");
			m_scanners.pop_back();
			if (!m_scanners.empty()) m_result.emplace_back(m_scanners.back().loc());
			break;
		case TOKEN_IDENTIFIER:
			if (auto it = m_macros.find(tok.str()); it != m_macros.end()) {
				expand_macro(tok.str(), it->second);
				m_result.emplace_back(sc.loc());
				continue;
			}
			[[fallthrough]];
		default:
			m_result.push_back(std::move(tok));
		}
	}
}

void preprocessor::expand_macro(const std::string& name, const macro& m) {
	auto& sc = m_scanners.back();
	std::vector<std::vector<token>> args;

	if (!m.params.empty() || sc.peek().type() == TOKEN_LPAREN) {
		if (!sc.match(TOKEN_LPAREN)) error("expected '(' for macro " + name);
		if (!sc.match(TOKEN_RPAREN)) {
			do {
				std::vector<token> arg;
				int depth = 0;
				while ((sc.peek().type() != TOKEN_COMMA && sc.peek().type() != TOKEN_RPAREN) || depth > 0) {
					if (sc.peek().type() == TOKEN_LPAREN) ++depth;
					if (sc.peek().type() == TOKEN_RPAREN) --depth;
					if (sc.peek().type() == TOKEN_END) error("unexpected EOF in macro args");
					arg.push_back(sc.scan());
				}
				args.push_back(std::move(arg));
			} while (sc.match(TOKEN_COMMA));
			if (!sc.match(TOKEN_RPAREN)) error("expected ')' after macro args");
		}
	}

	if (args.size() != m.params.size())
		error("macro " + name + " expects " + std::to_string(m.params.size()) + " args");

	std::map<std::string, size_t> param_idx;
	for (size_t i = 0; i < m.params.size(); ++i) param_idx[m.params[i]] = i;

	std::vector<token> expanded;
	for (const auto& t : m.body) {
		if (t.type() == TOKEN_IDENTIFIER) {
			if (auto it = param_idx.find(t.str()); it != param_idx.end()) {
				for (const auto& a : args[it->second]) expanded.push_back(a);
				continue;
			}
		} else if (t.type() == TOKEN_PP_STRINGIFY) {
			if (auto it = param_idx.find(t.str()); it != param_idx.end()) {
				std::string s;
				for (const auto& a : args[it->second]) s += token_str(a);
				expanded.emplace_back(TOKEN_STRING_LITERAL, std::move(s), t.column());
				continue;
			}
			error("unknown stringify param: " + t.str());
		}
		expanded.push_back(t);
	}

	sc.inject(std::move(expanded));
}

}
