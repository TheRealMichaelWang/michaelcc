#ifndef MICHAELCC_SYNTAX_HPP
#define MICHAELCC_SYNTAX_HPP

#include "errors.hpp"
#include "tokens.hpp"
#include <memory>
#include <vector>
#include <optional>
#include <string>

namespace michaelcc::syntax {

using ptr = std::unique_ptr<struct node>;

struct node {
	source_location loc;
	explicit node(source_location l = {}) : loc(std::move(l)) {}
	virtual ~node() = default;
	node(const node&) = delete;
	node& operator=(const node&) = delete;
	node(node&&) = default;
	node& operator=(node&&) = default;
};

// Types
struct void_type : node { using node::node; };

struct int_type : node {
	enum class kind { CHAR, SHORT, INT, LONG };
	uint8_t quals;
	kind cls;
	int_type(uint8_t q, kind c, source_location l) : node(std::move(l)), quals(q), cls(c) {}
};

struct float_type : node {
	enum class kind { FLOAT, DOUBLE };
	kind cls;
	float_type(kind c, source_location l) : node(std::move(l)), cls(c) {}
};

struct pointer_type : node {
	ptr pointee;
	pointer_type(ptr p, source_location l) : node(std::move(l)), pointee(std::move(p)) {}
};

struct array_type : node {
	ptr elem;
	ptr len;
	array_type(ptr e, ptr l, source_location loc) : node(std::move(loc)), elem(std::move(e)), len(std::move(l)) {}
};

struct func_type : node {
	ptr ret;
	std::vector<ptr> params;
	func_type(ptr r, std::vector<ptr> p, source_location l) : node(std::move(l)), ret(std::move(r)), params(std::move(p)) {}
};

struct named_type : node {
			enum class kind { STRUCT, UNION, ENUM, TYPEDEF };
	kind k;
	std::string name;
	named_type(kind k, std::string n, source_location l) : node(std::move(l)), k(k), name(std::move(n)) {}
};

// Expressions
struct int_lit : node {
	uint64_t val;
	int_type::kind cls;
	uint8_t quals;
	int_lit(uint64_t v, uint8_t q, int_type::kind c, source_location l) : node(std::move(l)), val(v), quals(q), cls(c) {}
};

struct float_lit : node {
	double val;
	float_type::kind cls;
	float_lit(double v, float_type::kind c, source_location l) : node(std::move(l)), val(v), cls(c) {}
};

struct string_lit : node {
	std::string val;
	string_lit(std::string v, source_location l) : node(std::move(l)), val(std::move(v)) {}
};

struct char_lit : node {
	char val;
	char_lit(char v, source_location l) : node(std::move(l)), val(v) {}
};

struct ident : node {
	std::string name;
	ident(std::string n, source_location l) : node(std::move(l)), name(std::move(n)) {}
};

struct binary : node {
	token_type op;
	ptr lhs, rhs;
	binary(token_type o, ptr l, ptr r, source_location loc) : node(std::move(loc)), op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct unary : node {
	token_type op;
	ptr operand;
	bool prefix;
	unary(token_type o, ptr e, bool p, source_location l) : node(std::move(l)), op(o), operand(std::move(e)), prefix(p) {}
};

struct ternary : node {
	ptr cond, then_e, else_e;
	ternary(ptr c, ptr t, ptr e, source_location l) : node(std::move(l)), cond(std::move(c)), then_e(std::move(t)), else_e(std::move(e)) {}
};

struct assign : node {
	token_type op;
	ptr target, val;
	assign(token_type o, ptr t, ptr v, source_location l) : node(std::move(l)), op(o), target(std::move(t)), val(std::move(v)) {}
};

struct call : node {
	ptr callee;
	std::vector<ptr> args;
	call(ptr c, std::vector<ptr> a, source_location l) : node(std::move(l)), callee(std::move(c)), args(std::move(a)) {}
};

struct index : node {
	ptr arr, idx;
	index(ptr a, ptr i, source_location l) : node(std::move(l)), arr(std::move(a)), idx(std::move(i)) {}
};

struct member : node {
	ptr obj;
	std::string name;
	bool arrow;
	member(ptr o, std::string n, bool a, source_location l) : node(std::move(l)), obj(std::move(o)), name(std::move(n)), arrow(a) {}
};

struct cast : node {
	ptr type, operand;
	cast(ptr t, ptr o, source_location l) : node(std::move(l)), type(std::move(t)), operand(std::move(o)) {}
};

struct sizeof_expr : node {
	ptr operand;
	bool is_type;
	sizeof_expr(ptr o, bool t, source_location l) : node(std::move(l)), operand(std::move(o)), is_type(t) {}
};

struct init_list : node {
	std::vector<ptr> elems;
	init_list(std::vector<ptr> e, source_location l) : node(std::move(l)), elems(std::move(e)) {}
};

// Statements
struct expr_stmt : node {
	ptr expr;
	expr_stmt(ptr e, source_location l) : node(std::move(l)), expr(std::move(e)) {}
};

struct block : node {
	std::vector<ptr> stmts;
	block(std::vector<ptr> s, source_location l) : node(std::move(l)), stmts(std::move(s)) {}
};

struct if_stmt : node {
	ptr cond, then_b, else_b;
	if_stmt(ptr c, ptr t, ptr e, source_location l) : node(std::move(l)), cond(std::move(c)), then_b(std::move(t)), else_b(std::move(e)) {}
};

struct while_stmt : node {
	ptr cond, body;
	while_stmt(ptr c, ptr b, source_location l) : node(std::move(l)), cond(std::move(c)), body(std::move(b)) {}
};

struct do_stmt : node {
	ptr body, cond;
	do_stmt(ptr b, ptr c, source_location l) : node(std::move(l)), body(std::move(b)), cond(std::move(c)) {}
};

struct for_stmt : node {
	ptr init, cond, inc, body;
	for_stmt(ptr i, ptr c, ptr n, ptr b, source_location l) : node(std::move(l)), init(std::move(i)), cond(std::move(c)), inc(std::move(n)), body(std::move(b)) {}
};

struct switch_case {
	ptr val;
	std::vector<ptr> stmts;
};

struct switch_stmt : node {
	ptr expr;
	std::vector<switch_case> cases;
	switch_stmt(ptr e, std::vector<switch_case> c, source_location l) : node(std::move(l)), expr(std::move(e)), cases(std::move(c)) {}
};

struct return_stmt : node {
	ptr val;
	return_stmt(ptr v, source_location l) : node(std::move(l)), val(std::move(v)) {}
};

struct break_stmt : node { using node::node; };
struct continue_stmt : node { using node::node; };

struct goto_stmt : node {
	std::string label;
	goto_stmt(std::string l, source_location loc) : node(std::move(loc)), label(std::move(l)) {}
};

struct label_stmt : node {
	std::string label;
	ptr stmt;
	label_stmt(std::string l, ptr s, source_location loc) : node(std::move(loc)), label(std::move(l)), stmt(std::move(s)) {}
};

struct empty_stmt : node { using node::node; };

// Declarations
struct param {
	uint8_t quals = 0;
	ptr type;
	std::string name;
};

struct var_decl : node {
	uint8_t storage;
	ptr type;
	std::string name;
	ptr init;
	var_decl(uint8_t s, ptr t, std::string n, ptr i, source_location l)
		: node(std::move(l)), storage(s), type(std::move(t)), name(std::move(n)), init(std::move(i)) {}
};

struct func_decl : node {
	ptr ret;
	std::string name;
	std::vector<param> params;
	std::unique_ptr<block> body;
	func_decl(ptr r, std::string n, std::vector<param> p, std::unique_ptr<block> b, source_location l)
		: node(std::move(l)), ret(std::move(r)), name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
	bool has_body() const { return body != nullptr; }
};

struct struct_field {
	ptr type;
			std::string name;
	ptr bits;
};

struct struct_decl : node {
	std::optional<std::string> name;
	std::vector<struct_field> fields;
	struct_decl(std::optional<std::string> n, std::vector<struct_field> f, source_location l)
		: node(std::move(l)), name(std::move(n)), fields(std::move(f)) {}
	bool has_body() const { return !fields.empty(); }
};

struct union_decl : node {
	std::optional<std::string> name;
	std::vector<struct_field> fields;
	union_decl(std::optional<std::string> n, std::vector<struct_field> f, source_location l)
		: node(std::move(l)), name(std::move(n)), fields(std::move(f)) {}
};

		struct enumerator {
			std::string name;
	ptr val;
};

struct enum_decl : node {
	std::optional<std::string> name;
	std::vector<enumerator> values;
	enum_decl(std::optional<std::string> n, std::vector<enumerator> v, source_location l)
		: node(std::move(l)), name(std::move(n)), values(std::move(v)) {}
};

struct typedef_decl : node {
	ptr type;
	std::string name;
	typedef_decl(ptr t, std::string n, source_location l) : node(std::move(l)), type(std::move(t)), name(std::move(n)) {}
};

struct translation_unit : node {
	std::vector<ptr> decls;
	translation_unit(std::vector<ptr> d, source_location l) : node(std::move(l)), decls(std::move(d)) {}
};

}

#endif
