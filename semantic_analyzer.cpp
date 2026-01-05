#include "semantic_analyzer.hpp"

namespace michaelcc {

using namespace semantic;
namespace syn = syntax;

analyzer::analyzer() {
	m_void = m_unit.make_type<void_type>();
	m_char = m_unit.make_type<int_type>(int_type::kind::CHAR, false);
	m_int = m_unit.make_type<int_type>(int_type::kind::INT, false);
	m_long = m_unit.make_type<int_type>(int_type::kind::LONG, false);
	m_float = m_unit.make_type<float_type>(float_type::kind::FLOAT);
	m_double = m_unit.make_type<float_type>(float_type::kind::DOUBLE);
	m_char_ptr = m_unit.make_type<ptr_type>(m_char);
}

void analyzer::push_scope() { m_scopes.emplace_back(); }
void analyzer::pop_scope() { m_scopes.pop_back(); }

var_def* analyzer::lookup_var(const std::string& name) {
	for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
		auto i = it->vars.find(name);
		if (i != it->vars.end()) return i->second;
	}
	return m_unit.find_global(name);
}

void analyzer::declare_var(std::unique_ptr<var_def> v) {
	if (!m_scopes.empty()) {
		auto* p = v.get();
		m_scopes.back().owned.push_back(std::move(v));
		m_scopes.back().vars[p->name] = p;
	}
}

void analyzer::error(const std::string& msg, const source_location& loc) {
	throw compile_error(msg, loc);
}

type* analyzer::resolve_type(const syn::node* n) {
	if (auto* v = dynamic_cast<const syn::void_type*>(n)) return m_void;

	if (auto* i = dynamic_cast<const syn::int_type*>(n)) {
		bool u = i->quals & 0x08;
		int_type::kind k;
		switch (i->cls) {
		case syn::int_type::kind::CHAR: k = int_type::kind::CHAR; break;
		case syn::int_type::kind::SHORT: k = int_type::kind::SHORT; break;
		case syn::int_type::kind::LONG: k = int_type::kind::LONG; break;
		default: k = int_type::kind::INT;
		}
		return m_unit.make_type<int_type>(k, u);
	}

	if (auto* f = dynamic_cast<const syn::float_type*>(n)) {
		return f->cls == syn::float_type::kind::FLOAT ? m_float : m_double;
	}

	if (auto* p = dynamic_cast<const syn::pointer_type*>(n)) {
		return m_unit.make_type<ptr_type>(resolve_type(p->pointee.get()));
	}

	if (auto* a = dynamic_cast<const syn::array_type*>(n)) {
		auto* elem = resolve_type(a->elem.get());
		size_t count = 0;
		if (a->len) {
			auto v = eval_const(a->len.get());
			if (!v) error("array size must be constant", a->loc);
			count = static_cast<size_t>(*v);
		}
		return m_unit.make_type<array_type>(elem, count);
	}

	if (auto* nt = dynamic_cast<const syn::named_type*>(n))
		return resolve_named(nt);

	if (auto* ft = dynamic_cast<const syn::func_type*>(n)) {
		auto* ret = resolve_type(ft->ret.get());
		std::vector<type*> params;
		for (auto& p : ft->params) params.push_back(resolve_type(p.get()));
		return m_unit.make_type<func_type>(ret, std::move(params));
	}

	error("unknown type", n->loc);
}

type* analyzer::resolve_named(const syn::named_type* n) {
	switch (n->k) {
	case syn::named_type::kind::STRUCT:
		if (auto* s = m_unit.find_struct(n->name)) return s;
		error("unknown struct '" + n->name + "'", n->loc);
	case syn::named_type::kind::UNION:
		if (auto* u = m_unit.find_union(n->name)) return u;
		error("unknown union '" + n->name + "'", n->loc);
	case syn::named_type::kind::ENUM:
		if (auto* e = m_unit.find_enum(n->name)) return e;
		error("unknown enum '" + n->name + "'", n->loc);
	case syn::named_type::kind::TYPEDEF:
		if (auto* t = m_unit.find_typedef(n->name)) return t;
		error("unknown type '" + n->name + "'", n->loc);
	}
	error("unknown named type", n->loc);
}

void analyzer::compute_struct_layout(struct_type* st, const std::vector<syn::struct_field>& fields) {
	std::vector<field> flds;
	size_t off = 0, max_al = 1;

	for (auto& f : fields) {
		auto* t = resolve_type(f.type.get());
		size_t al = t->align();
		max_al = std::max(max_al, al);
		off = (off + al - 1) & ~(al - 1);

		field fd{f.name, t, off, 0, 0};
		if (f.bits) {
			if (auto v = eval_const(f.bits.get()))
				fd.bit_width = static_cast<size_t>(*v);
		}
		flds.push_back(fd);
		off += t->size();
	}

	size_t sz = (off + max_al - 1) & ~(max_al - 1);
	st->complete(std::move(flds), sz, max_al);
}

void analyzer::compute_union_layout(union_type* ut, const std::vector<syn::struct_field>& fields) {
	std::vector<field> flds;
	size_t max_sz = 0, max_al = 1;

	for (auto& f : fields) {
		auto* t = resolve_type(f.type.get());
		max_sz = std::max(max_sz, t->size());
		max_al = std::max(max_al, t->align());
		flds.push_back({f.name, t, 0, 0, 0});
	}

	size_t sz = (max_sz + max_al - 1) & ~(max_al - 1);
	ut->complete(std::move(flds), sz, max_al);
}

std::optional<int64_t> analyzer::eval_const(const syn::node* n) {
	if (auto* lit = dynamic_cast<const syn::int_lit*>(n))
		return static_cast<int64_t>(lit->val);

	if (auto* bin = dynamic_cast<const syn::binary*>(n)) {
		auto l = eval_const(bin->lhs.get());
		auto r = eval_const(bin->rhs.get());
		if (!l || !r) return std::nullopt;
		switch (bin->op) {
		case TOKEN_PLUS: return *l + *r;
		case TOKEN_MINUS: return *l - *r;
		case TOKEN_STAR: return *l * *r;
		case TOKEN_SLASH: return *r ? *l / *r : 0;
		case TOKEN_PERCENT: return *r ? *l % *r : 0;
		case TOKEN_LSHIFT: return *l << *r;
		case TOKEN_RSHIFT: return *l >> *r;
		case TOKEN_AMP: return *l & *r;
		case TOKEN_PIPE: return *l | *r;
		case TOKEN_CARET: return *l ^ *r;
		default: return std::nullopt;
		}
	}

	if (auto* u = dynamic_cast<const syn::unary*>(n)) {
		auto v = eval_const(u->operand.get());
		if (!v) return std::nullopt;
		switch (u->op) {
		case TOKEN_MINUS: return -*v;
		case TOKEN_TILDE: return ~*v;
		case TOKEN_BANG: return !*v;
		default: return std::nullopt;
		}
	}

	return std::nullopt;
}

expr_ptr analyzer::analyze_expr(const syn::node* n) {
	if (auto* lit = dynamic_cast<const syn::int_lit*>(n))
		return std::make_unique<int_const>(m_int, lit->val, n->loc);

	if (auto* lit = dynamic_cast<const syn::float_lit*>(n)) {
		auto* t = lit->cls == syn::float_type::kind::FLOAT ? static_cast<type*>(m_float) : m_double;
		return std::make_unique<float_const>(t, lit->val, n->loc);
	}

	if (auto* lit = dynamic_cast<const syn::string_lit*>(n)) {
		size_t id = m_unit.add_string(lit->val);
		return std::make_unique<string_const>(m_char_ptr, lit->val, id, n->loc);
	}

	if (auto* lit = dynamic_cast<const syn::char_lit*>(n))
		return std::make_unique<int_const>(m_char, static_cast<uint64_t>(lit->val), n->loc);

	if (auto* id = dynamic_cast<const syn::ident*>(n)) {
		if (auto* v = lookup_var(id->name))
			return std::make_unique<var_ref>(v->vtype, v, n->loc);
		if (auto* f = m_unit.find_func(id->name)) {
			auto* pt = m_unit.make_type<ptr_type>(f->ftype);
			return std::make_unique<int_const>(pt, 0, n->loc);
		}
		error("undefined '" + id->name + "'", n->loc);
	}

	if (auto* b = dynamic_cast<const syn::binary*>(n)) return analyze_binary(b);
	if (auto* u = dynamic_cast<const syn::unary*>(n)) return analyze_unary(u);
	if (auto* c = dynamic_cast<const syn::call*>(n)) return analyze_call(c);
	if (auto* m = dynamic_cast<const syn::member*>(n)) return analyze_member(m);
	if (auto* i = dynamic_cast<const syn::index*>(n)) return analyze_subscript(i);
	if (auto* a = dynamic_cast<const syn::assign*>(n)) return analyze_assign(a);
	if (auto* t = dynamic_cast<const syn::ternary*>(n)) return analyze_ternary(t);
	if (auto* c = dynamic_cast<const syn::cast*>(n)) return analyze_cast(c);
	if (auto* s = dynamic_cast<const syn::sizeof_expr*>(n)) return analyze_sizeof(s);
	if (auto* i = dynamic_cast<const syn::init_list*>(n)) return analyze_init_list(i);

	error("unsupported expression", n->loc);
}

expr_ptr analyzer::analyze_binary(const syn::binary* n) {
	auto lhs = analyze_expr(n->lhs.get());
	auto rhs = analyze_expr(n->rhs.get());
	auto* t = common_type(lhs->etype, rhs->etype);
	lhs = convert_to(std::move(lhs), t);
	rhs = convert_to(std::move(rhs), t);

	binary_expr::op op;
	switch (n->op) {
	case TOKEN_PLUS: op = binary_expr::op::ADD; break;
	case TOKEN_MINUS: op = binary_expr::op::SUB; break;
	case TOKEN_STAR: op = binary_expr::op::MUL; break;
	case TOKEN_SLASH: op = binary_expr::op::DIV; break;
	case TOKEN_PERCENT: op = binary_expr::op::MOD; break;
	case TOKEN_AMP: op = binary_expr::op::AND; break;
	case TOKEN_PIPE: op = binary_expr::op::OR; break;
	case TOKEN_CARET: op = binary_expr::op::XOR; break;
	case TOKEN_LSHIFT: op = binary_expr::op::SHL; break;
	case TOKEN_RSHIFT: op = binary_expr::op::SHR; break;
	case TOKEN_EQ: op = binary_expr::op::EQ; break;
	case TOKEN_NE: op = binary_expr::op::NE; break;
	case TOKEN_LT: op = binary_expr::op::LT; break;
	case TOKEN_LE: op = binary_expr::op::LE; break;
	case TOKEN_GT: op = binary_expr::op::GT; break;
	case TOKEN_GE: op = binary_expr::op::GE; break;
	case TOKEN_AMP_AMP: op = binary_expr::op::LAND; break;
	case TOKEN_PIPE_PIPE: op = binary_expr::op::LOR; break;
	default: error("unknown binary op", n->loc);
	}

	type* rt = t;
	if (op >= binary_expr::op::EQ) rt = m_int;
	return std::make_unique<binary_expr>(rt, op, std::move(lhs), std::move(rhs), n->loc);
}

expr_ptr analyzer::analyze_unary(const syn::unary* n) {
	auto e = analyze_expr(n->operand.get());
	unary_expr::op op;
	type* t = e->etype;
	valcat cat = valcat::RVALUE;

	switch (n->op) {
	case TOKEN_MINUS: op = unary_expr::op::NEG; break;
	case TOKEN_BANG: op = unary_expr::op::NOT; t = m_int; break;
	case TOKEN_TILDE: op = unary_expr::op::BITNOT; break;
	case TOKEN_STAR:
		op = unary_expr::op::DEREF;
		if (auto* p = dynamic_cast<ptr_type*>(e->etype)) {
			t = p->pointee;
			cat = valcat::LVALUE;
		} else error("cannot dereference non-pointer", n->loc);
		break;
	case TOKEN_AMP:
		op = unary_expr::op::ADDR;
		if (!e->lvalue()) error("cannot take address of rvalue", n->loc);
		t = m_unit.make_type<ptr_type>(e->etype);
		break;
	case TOKEN_INC:
		op = n->prefix ? unary_expr::op::PRE_INC : unary_expr::op::POST_INC;
		if (!e->lvalue()) error("increment requires lvalue", n->loc);
		break;
	case TOKEN_DEC:
		op = n->prefix ? unary_expr::op::PRE_DEC : unary_expr::op::POST_DEC;
		if (!e->lvalue()) error("decrement requires lvalue", n->loc);
		break;
	default: error("unknown unary op", n->loc);
	}

	return std::make_unique<unary_expr>(t, cat, op, std::move(e), n->loc);
}

expr_ptr analyzer::analyze_call(const syn::call* n) {
	if (auto* id = dynamic_cast<const syn::ident*>(n->callee.get())) {
		if (auto* f = m_unit.find_func(id->name)) {
			std::vector<expr_ptr> args;
			auto& params = f->ftype->params;
			for (size_t i = 0; i < n->args.size(); ++i) {
				auto a = analyze_expr(n->args[i].get());
				if (i < params.size()) a = convert_to(std::move(a), params[i]);
				args.push_back(std::move(a));
			}
			return std::make_unique<call_expr>(f->ftype->ret, f, std::move(args), n->loc);
		}
		error("undefined function '" + id->name + "'", n->loc);
	}

	auto callee = analyze_expr(n->callee.get());
	auto* pt = dynamic_cast<ptr_type*>(callee->etype);
	if (!pt) error("callee is not a function", n->loc);
	auto* ft = dynamic_cast<func_type*>(pt->pointee);
	if (!ft) error("callee is not a function pointer", n->loc);

	std::vector<expr_ptr> args;
	for (auto& a : n->args) args.push_back(analyze_expr(a.get()));
	return std::make_unique<call_expr>(ft->ret, std::move(callee), std::move(args), n->loc);
}

expr_ptr analyzer::analyze_member(const syn::member* n) {
	auto obj = analyze_expr(n->obj.get());
	type* t = obj->etype;

	if (n->arrow) {
		auto* p = dynamic_cast<ptr_type*>(t);
		if (!p) error("arrow requires pointer", n->loc);
		t = p->pointee;
	}

	const field* f = nullptr;
	if (auto* st = dynamic_cast<struct_type*>(t)) f = st->find(n->name);
	else if (auto* ut = dynamic_cast<union_type*>(t)) {
		for (auto& m : ut->members) if (m.name == n->name) { f = &m; break; }
	} else error("member access on non-struct/union", n->loc);

	if (!f) error("no member '" + n->name + "'", n->loc);
	valcat cat = obj->lvalue() ? valcat::LVALUE : valcat::RVALUE;
	return std::make_unique<member_expr>(f->ftype, cat, std::move(obj), f, n->arrow, n->loc);
}

expr_ptr analyzer::analyze_subscript(const syn::index* n) {
	auto arr = analyze_expr(n->arr.get());
	auto idx = analyze_expr(n->idx.get());

	type* elem = nullptr;
	if (auto* a = dynamic_cast<array_type*>(arr->etype)) elem = a->elem;
	else if (auto* p = dynamic_cast<ptr_type*>(arr->etype)) elem = p->pointee;
	else error("subscript requires array or pointer", n->loc);

	if (!idx->etype->arithmetic()) error("index must be integer", n->loc);
	return std::make_unique<subscript_expr>(elem, std::move(arr), std::move(idx), n->loc);
}

expr_ptr analyzer::analyze_assign(const syn::assign* n) {
	auto tgt = analyze_expr(n->target.get());
	auto val = analyze_expr(n->val.get());
	if (!tgt->lvalue()) error("cannot assign to rvalue", n->loc);
	val = convert_to(std::move(val), tgt->etype);

	assign_expr::op op;
	switch (n->op) {
	case TOKEN_ASSIGN: op = assign_expr::op::ASSIGN; break;
	case TOKEN_PLUS_ASSIGN: op = assign_expr::op::ADD; break;
	case TOKEN_MINUS_ASSIGN: op = assign_expr::op::SUB; break;
	case TOKEN_STAR_ASSIGN: op = assign_expr::op::MUL; break;
	case TOKEN_SLASH_ASSIGN: op = assign_expr::op::DIV; break;
	case TOKEN_PERCENT_ASSIGN: op = assign_expr::op::MOD; break;
	case TOKEN_AMP_ASSIGN: op = assign_expr::op::AND; break;
	case TOKEN_PIPE_ASSIGN: op = assign_expr::op::OR; break;
	case TOKEN_CARET_ASSIGN: op = assign_expr::op::XOR; break;
	case TOKEN_LSHIFT_ASSIGN: op = assign_expr::op::SHL; break;
	case TOKEN_RSHIFT_ASSIGN: op = assign_expr::op::SHR; break;
	default: error("unknown assign op", n->loc);
	}

	return std::make_unique<assign_expr>(tgt->etype, op, std::move(tgt), std::move(val), n->loc);
}

expr_ptr analyzer::analyze_ternary(const syn::ternary* n) {
	auto cond = analyze_expr(n->cond.get());
	auto then_e = analyze_expr(n->then_e.get());
	auto else_e = analyze_expr(n->else_e.get());
	auto* t = common_type(then_e->etype, else_e->etype);
	then_e = convert_to(std::move(then_e), t);
	else_e = convert_to(std::move(else_e), t);
	return std::make_unique<cond_expr>(t, valcat::RVALUE, std::move(cond), std::move(then_e), std::move(else_e), n->loc);
}

expr_ptr analyzer::analyze_cast(const syn::cast* n) {
	auto* t = resolve_type(n->type.get());
	auto e = analyze_expr(n->operand.get());
	return std::make_unique<cast_expr>(t, std::move(e), false, n->loc);
}

expr_ptr analyzer::analyze_sizeof(const syn::sizeof_expr* n) {
	size_t sz;
	if (n->is_type) sz = resolve_type(n->operand.get())->size();
	else sz = analyze_expr(n->operand.get())->etype->size();
	auto* t = m_unit.make_type<int_type>(int_type::kind::LONG, true);
	return std::make_unique<semantic::sizeof_expr>(t, sz, n->loc);
}

expr_ptr analyzer::analyze_init_list(const syn::init_list* n) {
	std::vector<expr_ptr> elems;
	for (auto& e : n->elems) elems.push_back(analyze_expr(e.get()));
	return std::make_unique<init_list_expr>(m_void, std::move(elems), n->loc);
}

expr_ptr analyzer::convert_to(expr_ptr e, type* target) {
	if (e->etype->eq(*target)) return e;
	return std::make_unique<cast_expr>(target, std::move(e), true, source_location());
}

type* analyzer::common_type(type* a, type* b) {
	if (dynamic_cast<float_type*>(a) || dynamic_cast<float_type*>(b)) {
		auto* fa = dynamic_cast<float_type*>(a);
		auto* fb = dynamic_cast<float_type*>(b);
		if (fa && fa->k == float_type::kind::DOUBLE) return a;
		if (fb && fb->k == float_type::kind::DOUBLE) return b;
		if (fa) return a;
		if (fb) return b;
	}
	if (a->size() > b->size()) return a;
	if (b->size() > a->size()) return b;
	if (auto* ia = dynamic_cast<int_type*>(a); ia && ia->is_unsigned) return a;
	if (auto* ib = dynamic_cast<int_type*>(b); ib && ib->is_unsigned) return b;
	return a;
}

stmt_ptr analyzer::analyze_stmt(const syn::node* n) {
	if (auto* e = dynamic_cast<const syn::expr_stmt*>(n)) {
		auto ex = e->expr ? analyze_expr(e->expr.get()) : nullptr;
		return std::make_unique<expr_stmt>(std::move(ex), n->loc);
	}

	if (auto* b = dynamic_cast<const syn::block*>(n)) return analyze_block(b);

	if (auto* i = dynamic_cast<const syn::if_stmt*>(n)) {
		auto cond = analyze_expr(i->cond.get());
		auto then_b = analyze_stmt(i->then_b.get());
		stmt_ptr else_b;
		if (i->else_b) else_b = analyze_stmt(i->else_b.get());
		return std::make_unique<if_stmt>(std::move(cond), std::move(then_b), std::move(else_b), n->loc);
	}

	if (auto* w = dynamic_cast<const syn::while_stmt*>(n)) {
		auto cond = analyze_expr(w->cond.get());
		auto body = analyze_stmt(w->body.get());
		return std::make_unique<while_stmt>(std::move(cond), std::move(body), n->loc);
	}

	if (auto* d = dynamic_cast<const syn::do_stmt*>(n)) {
		auto body = analyze_stmt(d->body.get());
		auto cond = analyze_expr(d->cond.get());
		return std::make_unique<do_stmt>(std::move(body), std::move(cond), n->loc);
	}

	if (auto* f = dynamic_cast<const syn::for_stmt*>(n)) {
		push_scope();
		stmt_ptr init;
		if (f->init) init = analyze_stmt(f->init.get());
		expr_ptr cond, inc;
		if (f->cond) cond = analyze_expr(f->cond.get());
		if (f->inc) inc = analyze_expr(f->inc.get());
		auto body = analyze_stmt(f->body.get());
		pop_scope();
		return std::make_unique<for_stmt>(std::move(init), std::move(cond), std::move(inc), std::move(body), n->loc);
	}

	if (auto* s = dynamic_cast<const syn::switch_stmt*>(n)) {
		auto e = analyze_expr(s->expr.get());
		std::vector<switch_label> labels;
		std::vector<stmt_ptr> stmts;

		for (auto& c : s->cases) {
			std::optional<int64_t> val;
			if (c.val) {
				auto v = eval_const(c.val.get());
				if (!v) error("case value must be constant", c.val->loc);
				val = *v;
			}
			labels.push_back({val, stmts.size()});
			for (auto& st : c.stmts) stmts.push_back(analyze_stmt(st.get()));
		}

		auto body = std::make_unique<block_stmt>(std::move(stmts), n->loc);
		return std::make_unique<switch_stmt>(std::move(e), std::move(labels), std::move(body), n->loc);
	}

	if (auto* r = dynamic_cast<const syn::return_stmt*>(n)) {
		expr_ptr val;
		if (r->val) {
			val = analyze_expr(r->val.get());
			if (m_current_func) val = convert_to(std::move(val), m_current_func->ret);
		}
		return std::make_unique<return_stmt>(std::move(val), n->loc);
	}

	if (dynamic_cast<const syn::break_stmt*>(n))
		return std::make_unique<semantic::break_stmt>(n->loc);

	if (dynamic_cast<const syn::continue_stmt*>(n))
		return std::make_unique<semantic::continue_stmt>(n->loc);

	if (auto* g = dynamic_cast<const syn::goto_stmt*>(n))
		return std::make_unique<goto_stmt>(g->label, n->loc);

	if (auto* l = dynamic_cast<const syn::label_stmt*>(n)) {
		auto s = analyze_stmt(l->stmt.get());
		return std::make_unique<label_stmt>(l->label, std::move(s), n->loc);
	}

	if (auto* v = dynamic_cast<const syn::var_decl*>(n)) {
		analyze_var(v, false);
		return std::make_unique<expr_stmt>(nullptr, n->loc);
	}

	if (dynamic_cast<const syn::empty_stmt*>(n))
		return std::make_unique<expr_stmt>(nullptr, n->loc);

	error("unsupported statement", n->loc);
}

std::unique_ptr<block_stmt> analyzer::analyze_block(const syn::block* n) {
	push_scope();
	std::vector<stmt_ptr> stmts;
	for (auto& s : n->stmts) stmts.push_back(analyze_stmt(s.get()));
	pop_scope();
	return std::make_unique<block_stmt>(std::move(stmts), n->loc);
}

void analyzer::analyze_decl(const syn::node* n) {
	if (auto* f = dynamic_cast<const syn::func_decl*>(n)) analyze_func(f);
	else if (auto* v = dynamic_cast<const syn::var_decl*>(n)) analyze_var(v, true);
	else if (auto* s = dynamic_cast<const syn::struct_decl*>(n)) analyze_struct(s);
	else if (auto* u = dynamic_cast<const syn::union_decl*>(n)) analyze_union(u);
	else if (auto* e = dynamic_cast<const syn::enum_decl*>(n)) analyze_enum(e);
	else if (auto* t = dynamic_cast<const syn::typedef_decl*>(n)) analyze_typedef(t);
}

void analyzer::analyze_func(const syn::func_decl* n) {
	auto* ret = resolve_type(n->ret.get());
	std::vector<type*> param_types;
	for (auto& p : n->params) param_types.push_back(resolve_type(p.type.get()));
	auto* ft = m_unit.make_type<func_type>(ret, std::move(param_types));

	if (auto* existing = m_unit.find_func(n->name)) {
		if (!existing->ftype->eq(*ft)) error("conflicting function declaration", n->loc);
		if (n->has_body() && !existing->has_body()) {
			m_current_func = ft;
			push_scope();
			for (auto& p : n->params) {
				auto* pt = resolve_type(p.type.get());
				if (!p.name.empty()) {
					auto v = std::make_unique<var_def>(p.name, pt, var_def::storage::AUTO, nullptr, n->loc);
					declare_var(std::move(v));
				}
			}
			existing->set_body(analyze_block(n->body.get()));
			pop_scope();
			m_current_func = nullptr;
		}
		return;
	}

	std::vector<param_def> params;
	for (auto& p : n->params) params.push_back({p.name, resolve_type(p.type.get())});

	auto f = std::make_unique<func_def>(n->name, ft, std::move(params), n->loc);
	auto* fptr = m_unit.add_func(std::move(f));

	if (n->has_body()) {
		m_current_func = ft;
		push_scope();
		for (auto& p : fptr->params) {
			if (!p.name.empty()) {
				auto v = std::make_unique<var_def>(p.name, p.ptype, var_def::storage::AUTO, nullptr, n->loc);
				declare_var(std::move(v));
			}
		}
		fptr->set_body(analyze_block(n->body.get()));
		pop_scope();
		m_current_func = nullptr;
	}
}

void analyzer::analyze_var(const syn::var_decl* n, bool global) {
	auto* t = resolve_type(n->type.get());
	expr_ptr init;
	if (n->init) {
		init = analyze_expr(n->init.get());
		init = convert_to(std::move(init), t);
	}

	var_def::storage s = var_def::storage::AUTO;
	if (n->storage & 0x08) s = var_def::storage::EXTERN;
	else if (n->storage & 0x10) s = var_def::storage::STATIC;
	else if (n->storage & 0x20) s = var_def::storage::REGISTER;

	auto v = std::make_unique<var_def>(n->name, t, s, std::move(init), n->loc);
	if (global) m_unit.add_global(std::move(v));
	else declare_var(std::move(v));
}

void analyzer::analyze_struct(const syn::struct_decl* n) {
	if (!n->name) return;

	auto* existing = m_unit.find_struct(*n->name);
	if (existing) {
		if (n->has_body()) compute_struct_layout(existing, n->fields);
		return;
	}

	auto* st = m_unit.make_type<struct_type>(*n->name);
	m_unit.add_struct(*n->name, st);
	if (n->has_body()) compute_struct_layout(st, n->fields);
}

void analyzer::analyze_union(const syn::union_decl* n) {
	if (!n->name) return;

	auto* existing = m_unit.find_union(*n->name);
	if (existing) {
		if (!n->fields.empty()) compute_union_layout(existing, n->fields);
		return;
	}

	auto* ut = m_unit.make_type<union_type>(*n->name);
	m_unit.add_union(*n->name, ut);
	if (!n->fields.empty()) compute_union_layout(ut, n->fields);
}

void analyzer::analyze_enum(const syn::enum_decl* n) {
	std::vector<enum_const> consts;
	int64_t val = 0;
	for (auto& e : n->values) {
		if (e.val) {
			auto v = eval_const(e.val.get());
			if (!v) error("enum value must be constant", n->loc);
			val = *v;
		}
		consts.push_back({e.name, val++});
	}

	if (n->name) {
		if (auto* existing = m_unit.find_enum(*n->name)) {
			if (!n->values.empty()) existing->complete(std::move(consts));
			return;
		}
		auto* et = m_unit.make_type<enum_type>(*n->name, std::move(consts));
		m_unit.add_enum(*n->name, et);
	}
}

void analyzer::analyze_typedef(const syn::typedef_decl* n) {
	type* t = nullptr;

	if (auto* sd = dynamic_cast<const syn::struct_decl*>(n->type.get())) {
		analyze_struct(sd);
		if (sd->name) t = m_unit.find_struct(*sd->name);
		else error("anonymous struct in typedef not yet supported", n->loc);
	} else if (auto* ud = dynamic_cast<const syn::union_decl*>(n->type.get())) {
		analyze_union(ud);
		if (ud->name) t = m_unit.find_union(*ud->name);
		else error("anonymous union in typedef not yet supported", n->loc);
	} else if (auto* ed = dynamic_cast<const syn::enum_decl*>(n->type.get())) {
		analyze_enum(ed);
		if (ed->name) t = m_unit.find_enum(*ed->name);
		else error("anonymous enum in typedef not yet supported", n->loc);
	} else {
		t = resolve_type(n->type.get());
	}

	m_unit.add_typedef(n->name, t);
}

unit analyzer::analyze(const syn::translation_unit& tu) {
	for (auto& d : tu.decls) {
		if (!d) continue;
		if (auto* s = dynamic_cast<const syn::struct_decl*>(d.get())) {
			if (s->name && !m_unit.find_struct(*s->name)) {
				auto* st = m_unit.make_type<struct_type>(*s->name);
				m_unit.add_struct(*s->name, st);
			}
		}
		if (auto* u = dynamic_cast<const syn::union_decl*>(d.get())) {
			if (u->name && !m_unit.find_union(*u->name)) {
				auto* ut = m_unit.make_type<union_type>(*u->name);
				m_unit.add_union(*u->name, ut);
			}
		}
		if (auto* e = dynamic_cast<const syn::enum_decl*>(d.get())) {
			if (e->name && !m_unit.find_enum(*e->name)) {
				auto* et = m_unit.make_type<enum_type>(*e->name);
				m_unit.add_enum(*e->name, et);
			}
		}
	}

	for (auto& d : tu.decls) {
		if (d) analyze_decl(d.get());
	}

	return std::move(m_unit);
}

}
