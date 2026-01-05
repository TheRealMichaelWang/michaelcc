#ifndef MICHAELCC_SEMANTIC_ANALYZER_HPP
#define MICHAELCC_SEMANTIC_ANALYZER_HPP

#include "syntax.hpp"
#include "semantic.hpp"
#include <unordered_map>
#include <vector>

namespace michaelcc {

class analyzer {
public:
	analyzer();
	semantic::unit analyze(const syntax::translation_unit& tu);

private:
	semantic::unit m_unit;

	struct scope {
		std::unordered_map<std::string, semantic::var_def*> vars;
		std::vector<std::unique_ptr<semantic::var_def>> owned;
	};
	std::vector<scope> m_scopes;

	semantic::func_type* m_current_func = nullptr;

	semantic::void_type* m_void = nullptr;
	semantic::int_type* m_char = nullptr;
	semantic::int_type* m_int = nullptr;
	semantic::int_type* m_long = nullptr;
	semantic::float_type* m_float = nullptr;
	semantic::float_type* m_double = nullptr;
	semantic::ptr_type* m_char_ptr = nullptr;

	void push_scope();
	void pop_scope();
	semantic::var_def* lookup_var(const std::string& name);
	void declare_var(std::unique_ptr<semantic::var_def> v);

	semantic::type* resolve_type(const syntax::node* n);
	semantic::type* resolve_named(const syntax::named_type* n);

	void compute_struct_layout(semantic::struct_type* st, const std::vector<syntax::struct_field>& fields);
	void compute_union_layout(semantic::union_type* ut, const std::vector<syntax::struct_field>& fields);

	semantic::expr_ptr analyze_expr(const syntax::node* n);
	semantic::expr_ptr analyze_binary(const syntax::binary* n);
	semantic::expr_ptr analyze_unary(const syntax::unary* n);
	semantic::expr_ptr analyze_call(const syntax::call* n);
	semantic::expr_ptr analyze_member(const syntax::member* n);
	semantic::expr_ptr analyze_subscript(const syntax::index* n);
	semantic::expr_ptr analyze_assign(const syntax::assign* n);
	semantic::expr_ptr analyze_ternary(const syntax::ternary* n);
	semantic::expr_ptr analyze_cast(const syntax::cast* n);
	semantic::expr_ptr analyze_sizeof(const syntax::sizeof_expr* n);
	semantic::expr_ptr analyze_init_list(const syntax::init_list* n);

	semantic::expr_ptr convert_to(semantic::expr_ptr e, semantic::type* target);
	semantic::type* common_type(semantic::type* a, semantic::type* b);

	semantic::stmt_ptr analyze_stmt(const syntax::node* n);
	std::unique_ptr<semantic::block_stmt> analyze_block(const syntax::block* n);

	void analyze_decl(const syntax::node* n);
	void analyze_func(const syntax::func_decl* n);
	void analyze_var(const syntax::var_decl* n, bool global);
	void analyze_struct(const syntax::struct_decl* n);
	void analyze_union(const syntax::union_decl* n);
	void analyze_enum(const syntax::enum_decl* n);
	void analyze_typedef(const syntax::typedef_decl* n);

	std::optional<int64_t> eval_const(const syntax::node* n);

	[[noreturn]] void error(const std::string& msg, const source_location& loc);
};

}

#endif
