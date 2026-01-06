#ifndef MICHAELCC_LOGICAL_IR_HPP
#define MICHAELCC_LOGICAL_IR_HPP

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "tokens.hpp"
#include "symbols.hpp"

namespace michaelcc {
	namespace logical_ir {
		class type;
		class expression;
		class statement;
		class variable;
		class function_definition;
		class type_context;
		class control_block;

		class type {
		public:
			virtual ~type() = default;
		};

		class void_type final : public type {};

		class integer_type final : public type {
		private:
			uint8_t m_bits;
			bool m_unsigned;
		public:
			integer_type(uint8_t bits, bool is_unsigned)
				: m_bits(bits), m_unsigned(is_unsigned) {}

			uint8_t bits() const noexcept { return m_bits; }
			bool is_unsigned() const noexcept { return m_unsigned; }
		};

		class floating_type final : public type {
		private:
			uint8_t m_bits;
		public:
			explicit floating_type(uint8_t bits) : m_bits(bits) {}

			uint8_t bits() const noexcept { return m_bits; }
		};

		class pointer_type final : public type {
		private:
			const type* m_pointee;
		public:
			explicit pointer_type(const type* pointee) : m_pointee(pointee) {}

			const type* pointee() const noexcept { return m_pointee; }
		};

		class array_type final : public type {
		private:
			const type* m_element;
			size_t m_length;
		public:
			array_type(const type* element, size_t length)
				: m_element(element), m_length(length) {}

			const type* element() const noexcept { return m_element; }
			size_t length() const noexcept { return m_length; }
		};

		class enum_type final : public type {
		public:
			struct enumerator {
				std::string name;
				int64_t value;
			};
		private:
			std::string m_name;
			std::vector<enumerator> m_enumerators;
			const integer_type* m_underlying_type;
		public:
			enum_type(std::string&& name, std::vector<enumerator>&& enumerators, const integer_type* underlying)
				: m_name(std::move(name)), m_enumerators(std::move(enumerators)), m_underlying_type(underlying) {}

			const std::string& name() const noexcept { return m_name; }
			const std::vector<enumerator>& enumerators() const noexcept { return m_enumerators; }
			const integer_type* underlying_type() const noexcept { return m_underlying_type; }
		};

		class function_type final : public type {
		private:
			const type* m_return_type;
			std::vector<const type*> m_parameter_types;
			bool m_variadic;
		public:
			function_type(const type* return_type, std::vector<const type*>&& parameter_types, bool variadic = false)
				: m_return_type(return_type), m_parameter_types(std::move(parameter_types)), m_variadic(variadic) {}

			const type* return_type() const noexcept { return m_return_type; }
			const std::vector<const type*>& parameter_types() const noexcept { return m_parameter_types; }
			bool is_variadic() const noexcept { return m_variadic; }
		};

		class type_context {
		private:
			std::vector<std::unique_ptr<type>> m_types;
			std::unordered_map<std::string, struct_type*> m_structs;
			std::unordered_map<std::string, union_type*> m_unions;
			std::unordered_map<std::string, enum_type*> m_enums;

			void_type* m_void;
			integer_type* m_i8;
			integer_type* m_i16;
			integer_type* m_i32;
			integer_type* m_i64;
			integer_type* m_u8;
			integer_type* m_u16;
			integer_type* m_u32;
			integer_type* m_u64;
			floating_type* m_f32;
			floating_type* m_f64;
			pointer_type* m_char_ptr;

			template<typename T, typename... Args>
			T* make(Args&&... args) {
				auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
				T* raw = ptr.get();
				m_types.push_back(std::move(ptr));
				return raw;
			}
		public:
			type_context() {
				m_void = make<void_type>();
				m_i8 = make<integer_type>(8, false);
				m_i16 = make<integer_type>(16, false);
				m_i32 = make<integer_type>(32, false);
				m_i64 = make<integer_type>(64, false);
				m_u8 = make<integer_type>(8, true);
				m_u16 = make<integer_type>(16, true);
				m_u32 = make<integer_type>(32, true);
				m_u64 = make<integer_type>(64, true);
				m_f32 = make<floating_type>(32);
				m_f64 = make<floating_type>(64);
				m_char_ptr = make<pointer_type>(m_i8);
			}

			const void_type* get_void() const noexcept { return m_void; }
			const integer_type* get_i8() const noexcept { return m_i8; }
			const integer_type* get_i16() const noexcept { return m_i16; }
			const integer_type* get_i32() const noexcept { return m_i32; }
			const integer_type* get_i64() const noexcept { return m_i64; }
			const integer_type* get_u8() const noexcept { return m_u8; }
			const integer_type* get_u16() const noexcept { return m_u16; }
			const integer_type* get_u32() const noexcept { return m_u32; }
			const integer_type* get_u64() const noexcept { return m_u64; }
			const floating_type* get_f32() const noexcept { return m_f32; }
			const floating_type* get_f64() const noexcept { return m_f64; }
			const pointer_type* get_char_ptr() const noexcept { return m_char_ptr; }

			const pointer_type* get_pointer(const type* pointee) {
				return make<pointer_type>(pointee);
			}

			const array_type* get_array(const type* element, size_t length) {
				return make<array_type>(element, length);
			}

			const function_type* get_function(const type* return_type, std::vector<const type*>&& params, bool variadic = false) {
				return make<function_type>(return_type, std::move(params), variadic);
			}

			const struct_type* declare_struct(std::string&& name, std::vector<struct_type::field>&& fields, size_t size, size_t align) {
				auto* t = make<struct_type>(std::move(name), std::move(fields), size, align);
				m_structs[t->name()] = t;
				return t;
			}

			const union_type* declare_union(std::string&& name, std::vector<union_type::member>&& members, size_t size, size_t align) {
				auto* t = make<union_type>(std::move(name), std::move(members), size, align);
				m_unions[t->name()] = t;
				return t;
			}

			const enum_type* declare_enum(std::string&& name, std::vector<enum_type::enumerator>&& enumerators, const integer_type* underlying) {
				auto* t = make<enum_type>(std::move(name), std::move(enumerators), underlying);
				m_enums[t->name()] = t;
				return t;
			}

			const struct_type* lookup_struct(const std::string& name) const {
				auto it = m_structs.find(name);
				return it != m_structs.end() ? it->second : nullptr;
			}

			const union_type* lookup_union(const std::string& name) const {
				auto it = m_unions.find(name);
				return it != m_unions.end() ? it->second : nullptr;
			}

			const enum_type* lookup_enum(const std::string& name) const {
				auto it = m_enums.find(name);
				return it != m_enums.end() ? it->second : nullptr;
			}

            void implement_struct_field_types(std::string&& name, const std::vector<type*>& field_types) {
                auto it = m_structs.find(name);
                if (it == m_structs.end()) {
                    throw std::runtime_error("Struct not found: " + name);
                }

                for (size_t i = 0; i < it->second->fields().size(); i++) {
                    it->second->m_fields[i].field_type = field_types[i];
                }
            }

            void implement_union_member_types(std::string&& name, const std::vector<type*>& member_types) {
                auto it = m_unions.find(name);
                if (it == m_unions.end()) {
                    throw std::runtime_error("Union not found: " + name);
                }

                for (size_t i = 0; i < it->second->m_members.size(); i++) {
                    it->second->m_members[i].member_type = member_types[i];
                }
            }
		};

		class variable final : public symbol {
		private:
			const type* m_type;
			control_block* m_context;
			bool m_is_global;
		public:
			variable(std::string&& name, const type* var_type, bool is_global, control_block* context = nullptr)
				: symbol(std::move(name)), m_type(var_type), m_is_global(is_global), m_context(context) {}

			const type* get_type() const noexcept { return m_type; }
			control_block* get_context() const noexcept { return m_context; }
			bool is_global() const noexcept { return m_is_global; }
		};

		class expression {
		public:
			virtual ~expression() = default;
			virtual const std::unique_ptr<type>& get_type(const type_context& ctx) const = 0;
		};

		class integer_constant final : public expression {
		private:
			uint64_t m_value;
			std::unique_ptr<integer_type> m_type;
		public:
			integer_constant(uint64_t value, std::unique_ptr<integer_type>&& int_type)
				: m_value(value), m_type(std::move(int_type)) {}

			uint64_t value() const noexcept { return m_value; }
			const std::unique_ptr<type>& get_type(const type_context& ctx) const override { return m_type; }
		};

		class floating_constant final : public expression {
		private:
			double m_value;
			const floating_type* m_type;
		public:
			floating_constant(double value, const floating_type* float_type)
				: m_value(value), m_type(float_type) {}

			double value() const noexcept { return m_value; }
			const type* get_type(const type_context&) const override { return m_type; }
		};

		class string_constant final : public expression {
		private:
			size_t m_index;
		public:
			explicit string_constant(size_t index) : m_index(index) {}

			size_t index() const noexcept { return m_index; }
			const type* get_type(const type_context& ctx) const override { return ctx.get_char_ptr(); }
		};

		class variable_reference final : public expression {
		private:
			variable* m_variable;
		public:
			explicit variable_reference(variable* var) : m_variable(var) {}

			variable* get_variable() const noexcept { return m_variable; }
			bool is_global() const noexcept { return m_variable->is_global(); }

			const type* get_type(const type_context&) const override {
				return m_variable->get_type();
			}
		};

		class binary_operation final : public expression {
		private:
			token_type m_operator;
			std::unique_ptr<expression> m_left;
			std::unique_ptr<expression> m_right;
			const type* m_result_type;
		public:
			binary_operation(token_type op,
				std::unique_ptr<expression>&& left,
				std::unique_ptr<expression>&& right,
				const type* result_type)
				: m_operator(op),
				m_left(std::move(left)),
				m_right(std::move(right)),
				m_result_type(result_type) {}

			token_type get_operator() const noexcept { return m_operator; }
			const expression* left() const noexcept { return m_left.get(); }
			const expression* right() const noexcept { return m_right.get(); }
			const type* get_type(const type_context&) const override { return m_result_type; }
		};

		class unary_operation final : public expression {
		private:
			token_type m_operator;
			std::unique_ptr<expression> m_operand;
		public:
			unary_operation(token_type op,
				std::unique_ptr<expression>&& operand)
				: m_operator(op),
				m_operand(std::move(operand)) { }

			token_type get_operator() const noexcept { return m_operator; }
			const expression* operand() const noexcept { return m_operand.get(); }
			const type* get_type(const type_context& ctx) const override { return operand()->get_type(ctx); }
		};

		class type_cast final : public expression {
		private:
			std::unique_ptr<expression> m_operand;
			const type* m_target_type;
		public:
			type_cast(std::unique_ptr<expression>&& operand, const type* target_type)
				: m_operand(std::move(operand)), m_target_type(target_type) {}

			const expression* operand() const noexcept { return m_operand.get(); }
			const type* get_type(const type_context&) const override { return m_target_type; }
		};

		class address_of final : public expression {
		private:
			std::unique_ptr<expression> m_operand;
		public:
			address_of(std::unique_ptr<expression>&& operand)
				: m_operand(std::move(operand)) {}

			const expression* operand() const noexcept { return m_operand.get(); }

			const type* get_type(const type_context& ctx) const override { 
                return ctx.get_pointer(operand()->get_type(ctx)); 
            }
		};

		class dereference final : public expression {
		private:
			std::unique_ptr<expression> m_operand;
		public:
			explicit dereference(std::unique_ptr<expression>&& operand) : m_operand(std::move(operand)) {}

			const expression* operand() const noexcept { return m_operand.get(); }

			const type* get_type(const type_context& ctx) const override {
				auto* ptr_type = dynamic_cast<const pointer_type*>(m_operand->get_type(ctx));
				return ptr_type ? ptr_type->pointee() : nullptr;
			}
		};

		class member_access final : public expression {
		private:
			std::unique_ptr<expression> m_base;
			size_t m_field_index;
			const type* m_field_type;
		public:
			member_access(std::unique_ptr<expression>&& base, size_t field_index, const type* field_type)
				: m_base(std::move(base)), m_field_index(field_index), m_field_type(field_type) {}

			const expression* base() const noexcept { return m_base.get(); }
			size_t field_index() const noexcept { return m_field_index; }
			const type* get_type(const type_context&) const override { return m_field_type; }
		};

		class array_index final : public expression {
		private:
			std::unique_ptr<expression> m_base;
			std::unique_ptr<expression> m_index;
		public:
			array_index(std::unique_ptr<expression>&& base, std::unique_ptr<expression>&& index)
				: m_base(std::move(base)), m_index(std::move(index)) {}

			const expression* base() const noexcept { return m_base.get(); }
			const expression* index() const noexcept { return m_index.get(); }

			const type* get_type(const type_context& ctx) const override {
				auto* base_type = m_base->get_type(ctx);
				if (auto* arr = dynamic_cast<const array_type*>(base_type)) {
					return arr->element();
				}
				if (auto* ptr = dynamic_cast<const pointer_type*>(base_type)) {
					return ptr->pointee();
				}
				return nullptr;
			}
		};

		class function_call final : public expression {
		private:
			std::unique_ptr<expression> m_callee;
			std::vector<std::unique_ptr<expression>> m_arguments;
		public:
			function_call(std::unique_ptr<expression>&& callee,
				std::vector<std::unique_ptr<expression>>&& arguments)
				: m_callee(std::move(callee)), m_arguments(std::move(arguments)) {}

			const expression* callee() const noexcept { return m_callee.get(); }
			const std::vector<std::unique_ptr<expression>>& arguments() const noexcept { return m_arguments; }

			const type* get_type(const type_context& ctx) const override {
				auto* callee_type = m_callee->get_type(ctx);
				if (auto* ptr = dynamic_cast<const pointer_type*>(callee_type)) {
					callee_type = ptr->pointee();
				}
				if (auto* fn = dynamic_cast<const function_type*>(callee_type)) {
					return fn->return_type();
				}
				return nullptr;
			}
		};

		class conditional_expression final : public expression {
		private:
			std::unique_ptr<expression> m_condition;
			std::unique_ptr<expression> m_then_expression;
			std::unique_ptr<expression> m_else_expression;
			const type* m_result_type;
		public:
			conditional_expression(std::unique_ptr<expression>&& condition,
				std::unique_ptr<expression>&& then_expression,
				std::unique_ptr<expression>&& else_expression,
				const type* result_type)
				: m_condition(std::move(condition)),
				m_then_expression(std::move(then_expression)),
				m_else_expression(std::move(else_expression)),
				m_result_type(result_type) {}

			const expression* condition() const noexcept { return m_condition.get(); }
			const expression* then_expression() const noexcept { return m_then_expression.get(); }
			const expression* else_expression() const noexcept { return m_else_expression.get(); }
			const type* get_type(const type_context&) const override { return m_result_type; }
		};

		class statement {
		public:
			virtual ~statement() = default;
		};

		class control_block final {
		private:
			std::vector<std::unique_ptr<statement>> m_statements;
			std::vector<std::unique_ptr<symbol>> m_locals;

		public:
			control_block() = default;
			control_block(std::vector<std::unique_ptr<statement>>&& statements, std::vector<std::unique_ptr<symbol>>&& locals)
				: m_statements(std::move(statements)), m_locals(std::move(locals)) {}

			const std::vector<std::unique_ptr<statement>>& statements() const noexcept { return m_statements; }
			const std::vector<std::unique_ptr<symbol>>& locals() const noexcept { return m_locals; }

            void implement_statements(std::vector<std::unique_ptr<statement>>&& statements) {
                if (!m_statements.empty()) {
                    throw std::runtime_error("Control block already implemented");
                }
                m_statements = std::move(statements);
            }
		};

		class expression_statement final : public statement {
		private:
			std::unique_ptr<expression> m_expression;
		public:
			explicit expression_statement(std::unique_ptr<expression>&& expr)
				: m_expression(std::move(expr)) {}

			const expression* get_expression() const noexcept { return m_expression.get(); }
		};

		class assignment_statement final : public statement {
		private:
			std::unique_ptr<expression> m_destination;
			std::unique_ptr<expression> m_value;
		public:
			assignment_statement(std::unique_ptr<expression>&& destination,
				std::unique_ptr<expression>&& value)
				: m_destination(std::move(destination)), m_value(std::move(value)) {}

			const expression* destination() const noexcept { return m_destination.get(); }
			const expression* value() const noexcept { return m_value.get(); }
		};

		class local_declaration final : public statement {
		private:
			variable* m_variable;
			std::unique_ptr<expression> m_initializer;
		public:
			local_declaration(variable* var, std::unique_ptr<expression>&& initializer = nullptr)
				: m_variable(var), m_initializer(std::move(initializer)) {}

			variable* get_variable() const noexcept { return m_variable; }
			const expression* initializer() const noexcept { return m_initializer.get(); }
		};

		class return_statement final : public statement {
		private:
			std::unique_ptr<expression> m_value;
		public:
			explicit return_statement(std::unique_ptr<expression>&& value = nullptr)
				: m_value(std::move(value)) {}

			const expression* value() const noexcept { return m_value.get(); }
		};

		class if_statement final : public statement {
		private:
			std::unique_ptr<expression> m_condition;
			control_block m_then_body;
			control_block m_else_body;
		public:
			if_statement(std::unique_ptr<expression>&& condition,
				control_block&& then_body,
				control_block&& else_body)
				: m_condition(std::move(condition)),
				m_then_body(std::move(then_body)),
				m_else_body(std::move(else_body)) {}

			const expression* condition() const noexcept { return m_condition.get(); }
			const control_block& then_body() const noexcept { return m_then_body; }
			const control_block& else_body() const noexcept { return m_else_body; }
		};

		class loop_statement final : public statement {
		private:
			control_block m_body;
			std::unique_ptr<expression> m_condition;
			bool m_check_condition_first;
		public:
			loop_statement(control_block&& body,
				std::unique_ptr<expression>&& condition,
				bool check_condition_first = true)
				: m_body(std::move(body)),
				m_condition(std::move(condition)),
				m_check_condition_first(check_condition_first) {}

			const control_block& body() const noexcept { return m_body; }
			const expression* condition() const noexcept { return m_condition.get(); }
			bool check_condition_first() const noexcept { return m_check_condition_first; }
		};

		class break_statement final : public statement {};
		class continue_statement final : public statement {};

		class function_definition final : public symbol {
		private:
			std::vector<variable*> m_parameters;
			control_block m_body;
		public:
			explicit function_definition(std::string&& name)
				: symbol(std::move(name)), m_body() {}

			function_definition(std::string&& name,
				std::vector<variable*>&& parameters,
				control_block&& body)
				: symbol(std::move(name)),
				m_parameters(std::move(parameters)),
				m_body(std::move(body)) {}

			const std::vector<variable*>& parameters() const noexcept { return m_parameters; }
			const control_block& body() const { return *m_body; }
			bool is_defined() const noexcept { return m_body.has_value(); }

			void define(std::vector<variable*>&& parameters, control_block&& body) {
				m_parameters = std::move(parameters);
				m_body = std::move(body);
			}
		};

		class translation_unit {
		private:
			type_context m_types;
			std::vector<std::unique_ptr<symbol>> m_symbols;
			std::unordered_map<std::string, symbol*> m_symbol_table;
			std::vector<std::string> m_strings;
		public:
			type_context& types() noexcept { return m_types; }
			const type_context& types() const noexcept { return m_types; }

			symbol* lookup(const std::string& name) const {
				auto it = m_symbol_table.find(name);
				return it != m_symbol_table.end() ? it->second : nullptr;
			}

			function_definition* add_function(std::unique_ptr<function_definition>&& fn) {
				auto* ptr = fn.get();
				m_symbol_table[ptr->name()] = ptr;
				m_symbols.push_back(std::move(fn));
				return ptr;
			}

			variable* add_global(std::unique_ptr<variable>&& var) {
				auto* ptr = var.get();
				m_symbol_table[ptr->name()] = ptr;
				m_symbols.push_back(std::move(var));
				return ptr;
			}

			size_t add_string(std::string&& str) {
				m_strings.push_back(std::move(str));
				return m_strings.size() - 1;
			}

			const std::string& get_string(size_t index) const { return m_strings[index]; }
			const std::vector<std::string>& strings() const noexcept { return m_strings; }

			const std::vector<std::unique_ptr<symbol>>& symbols() const noexcept { return m_symbols; }
		};
	}
}

#endif
