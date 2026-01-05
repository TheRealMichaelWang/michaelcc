#ifndef MICHAELCC_SEMANTIC_HPP
#define MICHAELCC_SEMANTIC_HPP

#include "errors.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace michaelcc::semantic {

struct type;
struct struct_type;
struct union_type;
struct enum_type;
struct func_type;
struct expr;
struct stmt;
struct func_def;
struct var_def;

using type_ptr = std::unique_ptr<type>;
using expr_ptr = std::unique_ptr<expr>;
using stmt_ptr = std::unique_ptr<stmt>;

struct type {
	virtual ~type() = default;
	virtual size_t size() const = 0;
	virtual size_t align() const = 0;
	virtual bool complete() const { return size() > 0; }
	virtual bool scalar() const { return false; }
	virtual bool arithmetic() const { return false; }
	virtual std::string str() const = 0;
	virtual bool eq(const type& o) const = 0;
};

struct void_type final : type {
	size_t size() const override { return 0; }
	size_t align() const override { return 1; }
	bool complete() const override { return false; }
	std::string str() const override { return "void"; }
	bool eq(const type& o) const override { return dynamic_cast<const void_type*>(&o); }
};

struct int_type final : type {
	enum class kind { CHAR, SHORT, INT, LONG, LLONG };
	kind k;
	bool is_unsigned;
	size_t sz;

	int_type(kind k, bool u) : k(k), is_unsigned(u) {
		switch (k) {
		case kind::CHAR: sz = 1; break;
		case kind::SHORT: sz = 2; break;
		case kind::INT: sz = 4; break;
		default: sz = 8;
		}
	}

	size_t size() const override { return sz; }
	size_t align() const override { return sz; }
	bool scalar() const override { return true; }
	bool arithmetic() const override { return true; }
	std::string str() const override;
	bool eq(const type& o) const override {
		auto* p = dynamic_cast<const int_type*>(&o);
		return p && p->k == k && p->is_unsigned == is_unsigned;
	}
};

struct float_type final : type {
	enum class kind { FLOAT, DOUBLE, LDOUBLE };
	kind k;
	size_t sz;

	explicit float_type(kind k) : k(k) {
		sz = k == kind::FLOAT ? 4 : k == kind::DOUBLE ? 8 : 16;
	}

	size_t size() const override { return sz; }
	size_t align() const override { return sz; }
	bool scalar() const override { return true; }
	bool arithmetic() const override { return true; }
	std::string str() const override;
	bool eq(const type& o) const override {
		auto* p = dynamic_cast<const float_type*>(&o);
		return p && p->k == k;
	}
};

struct ptr_type final : type {
	type* pointee;
	explicit ptr_type(type* p) : pointee(p) {}

	size_t size() const override { return 8; }
	size_t align() const override { return 8; }
	bool scalar() const override { return true; }
	std::string str() const override { return pointee->str() + "*"; }
	bool eq(const type& o) const override {
		auto* p = dynamic_cast<const ptr_type*>(&o);
		return p && pointee->eq(*p->pointee);
	}
};

struct array_type final : type {
	type* elem;
	size_t count;

	array_type(type* e, size_t n) : elem(e), count(n) {}

	size_t size() const override { return count ? elem->size() * count : 0; }
	size_t align() const override { return elem->align(); }
	std::string str() const override;
	bool eq(const type& o) const override {
		auto* p = dynamic_cast<const array_type*>(&o);
		return p && elem->eq(*p->elem) && count == p->count;
	}
};

struct field {
	std::string name;
	type* ftype;
	size_t offset;
	size_t bit_off = 0, bit_width = 0;
};

struct struct_type final : type {
	std::string name;
	std::vector<field> fields;
	size_t sz = 0, al = 1;
	bool is_complete = false;

	explicit struct_type(std::string n) : name(std::move(n)) {}

	void complete(std::vector<field> f, size_t s, size_t a) {
		fields = std::move(f); sz = s; al = a; is_complete = true;
	}

	const field* find(const std::string& n) const {
		for (auto& f : fields) if (f.name == n) return &f;
		return nullptr;
	}

	size_t size() const override { return sz; }
	size_t align() const override { return al; }
	bool complete() const override { return is_complete; }
	std::string str() const override { return name.empty() ? "struct <anon>" : "struct " + name; }
	bool eq(const type& o) const override { return this == &o; }
};

struct union_type final : type {
	std::string name;
	std::vector<field> members;
	size_t sz = 0, al = 1;
	bool is_complete = false;

	explicit union_type(std::string n) : name(std::move(n)) {}

	void complete(std::vector<field> m, size_t s, size_t a) {
		members = std::move(m); sz = s; al = a; is_complete = true;
	}

	size_t size() const override { return sz; }
	size_t align() const override { return al; }
	bool complete() const override { return is_complete; }
	std::string str() const override { return name.empty() ? "union <anon>" : "union " + name; }
	bool eq(const type& o) const override { return this == &o; }
};

struct enum_const { std::string name; int64_t val; };

struct enum_type final : type {
	std::string name;
	std::vector<enum_const> consts;
	bool is_complete = false;

	explicit enum_type(std::string n) : name(std::move(n)) {}
	enum_type(std::string n, std::vector<enum_const> c) : name(std::move(n)), consts(std::move(c)), is_complete(true) {}

	void complete(std::vector<enum_const> c) { consts = std::move(c); is_complete = true; }

	size_t size() const override { return 4; }
	size_t align() const override { return 4; }
	bool scalar() const override { return true; }
	bool arithmetic() const override { return true; }
	bool complete() const override { return is_complete; }
	std::string str() const override { return name.empty() ? "enum <anon>" : "enum " + name; }
	bool eq(const type& o) const override { return this == &o; }
};

struct func_type final : type {
	type* ret;
	std::vector<type*> params;
	bool variadic = false;

	func_type(type* r, std::vector<type*> p, bool v = false)
		: ret(r), params(std::move(p)), variadic(v) {}

	size_t size() const override { return 0; }
	size_t align() const override { return 1; }
	bool complete() const override { return false; }
	std::string str() const override;
	bool eq(const type& o) const override;
};

// Expressions
enum class valcat { LVALUE, RVALUE };

struct expr {
	type* etype;
	valcat cat;
	source_location loc;

	expr(type* t, valcat c, source_location l) : etype(t), cat(c), loc(std::move(l)) {}
	virtual ~expr() = default;
	bool lvalue() const { return cat == valcat::LVALUE; }
};

struct int_const : expr {
	uint64_t val;
	int_const(type* t, uint64_t v, source_location l) : expr(t, valcat::RVALUE, std::move(l)), val(v) {}
};

struct float_const : expr {
	double val;
	float_const(type* t, double v, source_location l) : expr(t, valcat::RVALUE, std::move(l)), val(v) {}
};

struct string_const : expr {
	std::string val;
	size_t id;
	string_const(type* t, std::string v, size_t i, source_location l)
		: expr(t, valcat::LVALUE, std::move(l)), val(std::move(v)), id(i) {}
};

struct var_ref : expr {
	var_def* var;
	var_ref(type* t, var_def* v, source_location l) : expr(t, valcat::LVALUE, std::move(l)), var(v) {}
};

struct binary_expr : expr {
	enum class op { ADD, SUB, MUL, DIV, MOD, AND, OR, XOR, SHL, SHR, EQ, NE, LT, LE, GT, GE, LAND, LOR };
	op o;
	expr_ptr lhs, rhs;
	binary_expr(type* t, op o, expr_ptr l, expr_ptr r, source_location loc)
		: expr(t, valcat::RVALUE, std::move(loc)), o(o), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct unary_expr : expr {
	enum class op { NEG, NOT, BITNOT, DEREF, ADDR, PRE_INC, PRE_DEC, POST_INC, POST_DEC };
	op o;
	expr_ptr operand;
	unary_expr(type* t, valcat c, op o, expr_ptr e, source_location l)
		: expr(t, c, std::move(l)), o(o), operand(std::move(e)) {}
};

struct cast_expr : expr {
	expr_ptr operand;
	bool implicit;
	cast_expr(type* t, expr_ptr e, bool i, source_location l)
		: expr(t, valcat::RVALUE, std::move(l)), operand(std::move(e)), implicit(i) {}
};

struct assign_expr : expr {
	enum class op { ASSIGN, ADD, SUB, MUL, DIV, MOD, AND, OR, XOR, SHL, SHR };
	op o;
	expr_ptr target, val;
	assign_expr(type* t, op o, expr_ptr tgt, expr_ptr v, source_location l)
		: expr(t, valcat::LVALUE, std::move(l)), o(o), target(std::move(tgt)), val(std::move(v)) {}
};

struct cond_expr : expr {
	expr_ptr cond, then_e, else_e;
	cond_expr(type* t, valcat c, expr_ptr co, expr_ptr th, expr_ptr el, source_location l)
		: expr(t, c, std::move(l)), cond(std::move(co)), then_e(std::move(th)), else_e(std::move(el)) {}
};

struct call_expr : expr {
	func_def* func = nullptr;
	expr_ptr callee;
	std::vector<expr_ptr> args;
	call_expr(type* t, func_def* f, std::vector<expr_ptr> a, source_location l)
		: expr(t, valcat::RVALUE, std::move(l)), func(f), args(std::move(a)) {}
	call_expr(type* t, expr_ptr c, std::vector<expr_ptr> a, source_location l)
		: expr(t, valcat::RVALUE, std::move(l)), callee(std::move(c)), args(std::move(a)) {}
};

struct subscript_expr : expr {
	expr_ptr arr, idx;
	subscript_expr(type* t, expr_ptr a, expr_ptr i, source_location l)
		: expr(t, valcat::LVALUE, std::move(l)), arr(std::move(a)), idx(std::move(i)) {}
};

struct member_expr : expr {
	expr_ptr obj;
	const field* fld;
	bool arrow;
	member_expr(type* t, valcat c, expr_ptr o, const field* f, bool ar, source_location l)
		: expr(t, c, std::move(l)), obj(std::move(o)), fld(f), arrow(ar) {}
};

struct sizeof_expr : expr {
	size_t val;
	sizeof_expr(type* t, size_t v, source_location l) : expr(t, valcat::RVALUE, std::move(l)), val(v) {}
};

struct init_list_expr : expr {
	std::vector<expr_ptr> elems;
	init_list_expr(type* t, std::vector<expr_ptr> e, source_location l)
		: expr(t, valcat::RVALUE, std::move(l)), elems(std::move(e)) {}
};

// Statements
struct stmt {
	source_location loc;
	explicit stmt(source_location l) : loc(std::move(l)) {}
	virtual ~stmt() = default;
};

struct expr_stmt : stmt {
	expr_ptr e;
	expr_stmt(expr_ptr ex, source_location l) : stmt(std::move(l)), e(std::move(ex)) {}
};

struct block_stmt : stmt {
	std::vector<stmt_ptr> stmts;
	block_stmt(std::vector<stmt_ptr> s, source_location l) : stmt(std::move(l)), stmts(std::move(s)) {}
};

struct if_stmt : stmt {
	expr_ptr cond;
	stmt_ptr then_b, else_b;
	if_stmt(expr_ptr c, stmt_ptr t, stmt_ptr e, source_location l)
		: stmt(std::move(l)), cond(std::move(c)), then_b(std::move(t)), else_b(std::move(e)) {}
};

struct while_stmt : stmt {
	expr_ptr cond;
	stmt_ptr body;
	while_stmt(expr_ptr c, stmt_ptr b, source_location l)
		: stmt(std::move(l)), cond(std::move(c)), body(std::move(b)) {}
};

struct do_stmt : stmt {
	stmt_ptr body;
	expr_ptr cond;
	do_stmt(stmt_ptr b, expr_ptr c, source_location l)
		: stmt(std::move(l)), body(std::move(b)), cond(std::move(c)) {}
};

struct for_stmt : stmt {
	stmt_ptr init;
	expr_ptr cond, inc;
	stmt_ptr body;
	for_stmt(stmt_ptr i, expr_ptr c, expr_ptr n, stmt_ptr b, source_location l)
		: stmt(std::move(l)), init(std::move(i)), cond(std::move(c)), inc(std::move(n)), body(std::move(b)) {}
};

struct switch_label { std::optional<int64_t> val; size_t idx; };

struct switch_stmt : stmt {
	expr_ptr e;
	std::vector<switch_label> labels;
	std::unique_ptr<block_stmt> body;
	switch_stmt(expr_ptr ex, std::vector<switch_label> lbl, std::unique_ptr<block_stmt> b, source_location l)
		: stmt(std::move(l)), e(std::move(ex)), labels(std::move(lbl)), body(std::move(b)) {}
};

struct return_stmt : stmt {
	expr_ptr val;
	return_stmt(expr_ptr v, source_location l) : stmt(std::move(l)), val(std::move(v)) {}
};

struct break_stmt : stmt { using stmt::stmt; };
struct continue_stmt : stmt { using stmt::stmt; };

struct goto_stmt : stmt {
	std::string label;
	goto_stmt(std::string l, source_location loc) : stmt(std::move(loc)), label(std::move(l)) {}
};

struct label_stmt : stmt {
	std::string label;
	stmt_ptr s;
	label_stmt(std::string l, stmt_ptr st, source_location loc)
		: stmt(std::move(loc)), label(std::move(l)), s(std::move(st)) {}
};

struct decl_stmt : stmt {
	var_def* var;
	decl_stmt(var_def* v, source_location l) : stmt(std::move(l)), var(v) {}
};

// Definitions
struct var_def {
	enum class storage { AUTO, STATIC, EXTERN, REGISTER };
	std::string name;
	type* vtype;
	storage stor;
	expr_ptr init;
	source_location loc;

	var_def(std::string n, type* t, storage s, expr_ptr i, source_location l)
		: name(std::move(n)), vtype(t), stor(s), init(std::move(i)), loc(std::move(l)) {}
};

struct param_def { std::string name; type* ptype; };

struct func_def {
	std::string name;
	func_type* ftype;
	std::vector<param_def> params;
	std::unique_ptr<block_stmt> body;
	source_location loc;

	func_def(std::string n, func_type* t, std::vector<param_def> p, source_location l)
		: name(std::move(n)), ftype(t), params(std::move(p)), loc(std::move(l)) {}

	bool has_body() const { return body != nullptr; }
	void set_body(std::unique_ptr<block_stmt> b) { body = std::move(b); }
};

// Translation unit
class unit {
	std::vector<type_ptr> m_types;
	std::unordered_map<std::string, struct_type*> m_structs;
	std::unordered_map<std::string, union_type*> m_unions;
	std::unordered_map<std::string, enum_type*> m_enums;
	std::unordered_map<std::string, type*> m_typedefs;

	std::vector<std::unique_ptr<func_def>> m_funcs;
	std::vector<std::unique_ptr<var_def>> m_globals;
	std::unordered_map<std::string, func_def*> m_func_map;
	std::unordered_map<std::string, var_def*> m_global_map;

	std::vector<std::string> m_strings;

public:
	template<typename T, typename... Args>
	T* make_type(Args&&... args) {
		auto p = std::make_unique<T>(std::forward<Args>(args)...);
		auto* raw = p.get();
		m_types.push_back(std::move(p));
		return raw;
	}

	void add_struct(const std::string& n, struct_type* t) { m_structs[n] = t; }
	void add_union(const std::string& n, union_type* t) { m_unions[n] = t; }
	void add_enum(const std::string& n, enum_type* t) { m_enums[n] = t; }
	void add_typedef(const std::string& n, type* t) { m_typedefs[n] = t; }

	struct_type* find_struct(const std::string& n) const { auto i = m_structs.find(n); return i != m_structs.end() ? i->second : nullptr; }
	union_type* find_union(const std::string& n) const { auto i = m_unions.find(n); return i != m_unions.end() ? i->second : nullptr; }
	enum_type* find_enum(const std::string& n) const { auto i = m_enums.find(n); return i != m_enums.end() ? i->second : nullptr; }
	type* find_typedef(const std::string& n) const { auto i = m_typedefs.find(n); return i != m_typedefs.end() ? i->second : nullptr; }

	func_def* add_func(std::unique_ptr<func_def> f) {
		auto* p = f.get();
		m_func_map[f->name] = p;
		m_funcs.push_back(std::move(f));
		return p;
	}

	var_def* add_global(std::unique_ptr<var_def> v) {
		auto* p = v.get();
		m_global_map[v->name] = p;
		m_globals.push_back(std::move(v));
		return p;
	}

	func_def* find_func(const std::string& n) const { auto i = m_func_map.find(n); return i != m_func_map.end() ? i->second : nullptr; }
	var_def* find_global(const std::string& n) const { auto i = m_global_map.find(n); return i != m_global_map.end() ? i->second : nullptr; }

	size_t add_string(std::string s) { m_strings.push_back(std::move(s)); return m_strings.size() - 1; }
	const std::vector<std::string>& strings() const { return m_strings; }

	const std::vector<std::unique_ptr<func_def>>& funcs() const { return m_funcs; }
	const std::vector<std::unique_ptr<var_def>>& globals() const { return m_globals; }
};

}

#endif
