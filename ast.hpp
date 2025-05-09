#pragma once

#include <map>
#include <memory>
#include <vector>
#include <optional>
#include <cstdint>
#include <string>
#include <sstream>

namespace michaelcc {
	namespace ast {
		enum {
			NO_STORAGE_QUALIFIER = 0,
			CONST_STORAGE_QUALIFIER = 1,
			VOLATILE_STORAGE_QUALIFIER = 2,
			RESTRICT_STORAGE_QUALIFIER = 4,
			EXTERN_STORAGE_QUALIFIER = 8,
			STATIC_STORAGE_QUALIFIER = 16,
			REGISTER_STORAGE_QUALIFIER = 32
		};

		enum {
			NO_INT_QUALIFIER = 0,
			LONG_INT_QUALIFIER = 1,
			SIGNED_INT_QUALIFIER = 4,
			UNSIGNED_INT_QUALIFIER = 8
		};

		enum int_class {
			CHAR_INT_CLASS,
			SHORT_INT_CLASS,
			INT_INT_CLASS,
			LONG_INT_CLASS
		};

		enum float_class {
			FLOAT_FLOAT_CLASS,
			DOUBLE_FLOAT_CLASS
		};

		class ast_element {
		private:
			source_location m_location;

		public:
			explicit ast_element(source_location&& location) : m_location(std::move(location)) { 
			
			}

			ast_element() : m_location() { }

			const source_location location() const noexcept {
				return m_location;
			}

			virtual ~ast_element() = default;

			virtual void build_c_string(std::stringstream&) const = 0;

			const std::string to_string() const {
				std::stringstream ss;
				build_c_string(ss);
				return ss.str();
			}
		};

		class top_level_element : virtual public ast_element {
		public:
			void build_c_string(std::stringstream& ss) const override = 0;
		};

		class statement : virtual public ast_element {
		public:
			virtual void build_c_string_indent(std::stringstream& ss, int parent_precedence) const = 0;

			void build_c_string(std::stringstream& ss) const override {
				build_c_string_indent(ss, 0);
			}
		};

		class expression : virtual public ast_element {
		public:
			virtual void build_c_string_prec(std::stringstream& ss, int indent) const = 0;

			void build_c_string(std::stringstream& ss) const override {
				build_c_string_prec(ss, 0);
			}

			virtual std::unique_ptr<expression> clone() const = 0;
		};

		class set_destination : public expression {
		public:
			void build_c_string_prec(std::stringstream& ss, int indent) const override = 0;

			std::unique_ptr<expression> clone() const override = 0;
		};

		class type : virtual public ast_element {
		public:
			virtual void build_declarator(std::stringstream& ss, const std::string& identifier) const {
				build_c_string(ss);
				ss << ' ' << identifier;
			}

			virtual std::unique_ptr<type> clone() const = 0;
		};

		class void_type : public type {
		public:
			void_type(source_location&& location)
				: ast_element(std::move(location)) { }

			void build_c_string(std::stringstream& ss) const override {
				ss << "void";
			}

			std::unique_ptr<type> clone() const override {
				return std::make_unique<void_type>(source_location(location()));
			}
		};

		class int_type : public type {
		private:
			uint8_t m_qualifiers;
			int_class m_class;
		public:
			int_type(uint8_t qualifiers, int_class m_class, source_location&& location)
				: ast_element(std::move(location)),
				m_qualifiers(qualifiers), m_class(m_class) { }

			void build_c_string(std::stringstream&) const override;

			std::unique_ptr<type> clone() const override {
				return std::make_unique<int_type>(m_qualifiers, m_class, source_location(location()));
			}
		};

		class float_type : public type {
		private:
			float_class m_class;

		protected:
			void build_c_string(std::stringstream&) const override;

		public:
			explicit float_type(float_class m_class, source_location&& location) 
				: ast_element(std::move(location)),
				m_class(m_class) { }

			std::unique_ptr<type> clone() const override {
				return std::make_unique<float_type>(m_class, source_location(location()));
			}
		};

		class pointer_type : public type {
		private:
			std::unique_ptr<type> m_pointee_type;

		public:
			pointer_type(std::unique_ptr<type>&& pointee_type, source_location&& location)
				: ast_element(std::move(location)),
				m_pointee_type(std::move(pointee_type)) { }

			const type* pointee_type() const noexcept { return m_pointee_type.get(); }

			void build_c_string(std::stringstream&) const override;

			void build_declarator(std::stringstream& ss, const std::string& identifier) const override;

			std::unique_ptr<type> clone() const override {
				return std::make_unique<pointer_type>(m_pointee_type->clone(), source_location(location()));
			}
		};

		// Array type, e.g. int[10], float[]
		class array_type : public type {
		private:
			std::unique_ptr<type> m_element_type;
			std::optional<std::unique_ptr<expression>> m_length;
		
		public:
			array_type(std::unique_ptr<type>&& element_type, std::optional<std::unique_ptr<expression>>&& length, source_location&& location)
				: ast_element(std::move(location)),
				m_element_type(std::move(element_type)), m_length(std::move(length)) {}
			
			const type* element_type() const noexcept { return m_element_type.get(); }
			const std::optional<std::unique_ptr<expression>>& length() const noexcept { return m_length; }

			void build_c_string(std::stringstream&) const override;

			void build_declarator(std::stringstream& ss, const std::string& identifier) const override;

			std::unique_ptr<type> clone() const override {
				return std::make_unique<array_type>(m_element_type->clone(), m_length.has_value() ? std::make_optional(m_length.value()->clone()) : std::nullopt, source_location(location()));
			}
		}; 
		
		class function_pointer_type : public type {
		private:
			std::unique_ptr<type> m_return_type;
			std::vector<std::unique_ptr<type>> m_parameter_types;
		public:
			function_pointer_type(std::unique_ptr<type>&& return_type, std::vector<std::unique_ptr<type>>&& parameter_types, source_location&& location)
				: ast_element(std::move(location)), m_return_type(std::move(return_type)), m_parameter_types(std::move(parameter_types)) {}
			const type* return_type() const noexcept { return m_return_type.get(); }
			const std::vector<std::unique_ptr<type>>& parameter_types() const noexcept { return m_parameter_types; }
			
			void build_c_string(std::stringstream& ss) const override;

			void build_declarator(std::stringstream& ss, const std::string& identifier) const override;

			std::unique_ptr<type> clone() const override {
				std::vector<std::unique_ptr<ast::type>> parameter_types;
				parameter_types.reserve(m_parameter_types.size());
				for (const auto& parameter : m_parameter_types) {
					parameter_types.push_back(parameter->clone());
				}
				return std::make_unique<function_pointer_type>(m_return_type->clone(), std::move(parameter_types), source_location(location()));
			}
		};

		class context_block : public statement {
		private:
			std::vector<std::unique_ptr<statement>> m_statements;

		public:
			explicit context_block(std::vector<std::unique_ptr<statement>>&& statements, source_location&& location)
				: ast_element(std::move(location)), m_statements(std::move(statements)) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class for_loop final : public statement {
		private:
			std::unique_ptr<statement> m_initial_statement;
			std::unique_ptr<expression> m_condition;
			std::unique_ptr<statement> m_increment_statement;
			context_block m_to_execute;

		public:
			for_loop(std::unique_ptr<statement>&& initial_statement,
				std::unique_ptr<expression>&& condition,
				std::unique_ptr<statement>&& increment_statement,
				context_block&& to_execute, source_location&& location)
				: ast_element(std::move(location)),
				m_initial_statement(std::move(initial_statement)),
				m_condition(std::move(condition)),
				m_increment_statement(std::move(increment_statement)),
				m_to_execute(std::move(to_execute)) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class do_block final : public statement {
		private:
			std::unique_ptr<expression> m_condition;
			context_block m_to_execute;

		public:
			do_block(std::unique_ptr<expression>&& condition, context_block&& to_execute, source_location&& location)
				: ast_element(std::move(location)),
				m_condition(std::move(condition)),
				m_to_execute(std::move(to_execute)) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class while_block final : public statement {
		private:
			std::unique_ptr<expression> m_condition;
			context_block m_to_execute;

		public:
			while_block(std::unique_ptr<expression>&& condition, context_block&& to_execute, source_location&& location)
				: ast_element(std::move(location)),
				m_condition(std::move(condition)),
				m_to_execute(std::move(to_execute)) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class if_block final : public statement {
		private:
			std::unique_ptr<expression> m_condition;
			context_block m_execute_if_true;

		public:
			if_block(std::unique_ptr<expression>&& condition, context_block&& execute_if_true, source_location&& location)
				: ast_element(std::move(location)),
				m_condition(std::move(condition)),
				m_execute_if_true(std::move(execute_if_true)) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class if_else_block final : public statement {
		private:
			std::unique_ptr<expression> m_condition;
			context_block m_execute_if_true;
			context_block m_execute_if_false;

		public:
			if_else_block(std::unique_ptr<expression>&& condition,
				context_block&& execute_if_true,
				context_block&& execute_if_false,
				source_location&& location)
				: ast_element(std::move(location)),
				m_condition(std::move(condition)),
				m_execute_if_true(std::move(execute_if_true)),
				m_execute_if_false(std::move(execute_if_false)) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class return_statement final : public statement {
		private:
			std::unique_ptr<expression> value;

		public:
			explicit return_statement(std::unique_ptr<expression>&& return_value, source_location&& location)
				: ast_element(std::move(location)),
				value(std::move(return_value)) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class break_statement final : public statement {
		private:
			int loop_depth;

		public:
			explicit break_statement(int depth, source_location&& location) 
				: ast_element(std::move(location)),
				loop_depth(depth) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class continue_statement final : public statement {
		private:
			int loop_depth;

		public:
			explicit continue_statement(int depth, source_location&& location)
				: ast_element(std::move(location)), loop_depth(depth) { }

			void build_c_string_indent(std::stringstream&, int) const override;
		};

		class int_literal final : public expression {
		private:
			uint8_t m_qualifiers;
			int_class m_class;
			size_t m_value;

		public:
			explicit int_literal(uint8_t qualifiers, int_class i_class, size_t value, source_location&& location)
				: ast_element(std::move(location)),
				m_qualifiers(qualifiers),
				m_class(i_class),
				m_value(value) { }

			const uint8_t qualifiers() const noexcept {
				return m_qualifiers;
			}

			const int_class storage_class() const noexcept {
				return m_class;
			}

			const size_t unsigned_value() const noexcept {
				return m_value;
			}

			const int64_t signed_value() const noexcept {
				return static_cast<int64_t>(m_value);
			}

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<int_literal>(m_qualifiers, m_class, m_value, source_location(location()));
			}
		};

		class float_literal final : public expression {
		private:
			float m_value;

		public:
			explicit float_literal(float value, source_location&& location)
				: ast_element(std::move(location)),
				m_value(value) { }

			const float value() const noexcept {
				return m_value;
			}

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<float_literal>(m_value, source_location(location()));
			}
		};

		class double_literal final : public expression {
		private:
			double m_value;

		public:
			explicit double_literal(double value, source_location&& location)
				: ast_element(std::move(location)),
				m_value(value) { }

			const double value() const noexcept {
				return m_value;
			}

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<double_literal>(m_value, source_location(location()));
			}
		};

		class variable_reference final : public set_destination {
		private:
			std::string m_identifier;

		public:
			explicit variable_reference(const std::string& identifier, source_location&& location)
				: ast_element(std::move(location)),
				m_identifier(identifier) { }

			const std::string& identifier() const noexcept { return m_identifier; }

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<variable_reference>(m_identifier, source_location(location()));
			}
		};

		class get_index final : public set_destination {
		private:
			std::unique_ptr<set_destination> m_ptr;
			std::unique_ptr<expression> m_index;

		public:
			get_index(std::unique_ptr<set_destination>&& ptr, std::unique_ptr<expression>&& index, source_location&& location)
				: ast_element(std::move(location)),
				m_ptr(std::move(ptr)),
				m_index(std::move(index)) { }

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<get_index>(std::unique_ptr<set_destination>(dynamic_cast<set_destination*>(m_ptr->clone().release())), m_index->clone(), source_location(location()));
			}
		};

		class get_property final : public set_destination {
		private:
			std::unique_ptr<set_destination> m_struct;
			std::string m_property_name;
			bool m_is_pointer_dereference;

		public:
			get_property(std::unique_ptr<set_destination>&& m_struct, std::string&& property_name, bool is_pointer_dereference, source_location&& location)
				: ast_element(std::move(location)),
				m_struct(std::move(m_struct)),
				m_property_name(std::move(property_name)),
				m_is_pointer_dereference(is_pointer_dereference) { }

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<get_property>(std::unique_ptr<set_destination>(dynamic_cast<set_destination*>(m_struct->clone().release())), std::string(m_property_name), m_is_pointer_dereference, source_location(location()));
			}
		};

		class set_operator final : public statement, public expression {
		private:
			std::unique_ptr<set_destination> m_set_dest;
			std::unique_ptr<expression> m_set_value;

		public:
			set_operator(std::unique_ptr<set_destination>&& set_dest, std::unique_ptr<expression>&& set_value, source_location&& location)
				: ast_element(std::move(location)),
				m_set_dest(std::move(set_dest)),
				m_set_value(std::move(set_value)) { }

			void build_c_string_prec(std::stringstream&, int) const override;
			void build_c_string_indent(std::stringstream&, int) const override;

			void build_c_string(std::stringstream& ss) const override {
				build_c_string_indent(ss, 0);
			}

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<set_operator>(std::unique_ptr<set_destination>(dynamic_cast<set_destination*>(m_set_dest->clone().release())), m_set_value->clone(), source_location(location()));
			}
		};

		class dereference_operator final : public set_destination {
		private:
			std::unique_ptr<expression> m_pointer;

		public:
			explicit dereference_operator(std::unique_ptr<expression>&& pointer, source_location&& location)
				: ast_element(std::move(location)),
				m_pointer(std::move(pointer)) { }

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<dereference_operator>(m_pointer->clone(), source_location(location()));
			}
		};

		class get_reference final : public expression {
		private:
			std::unique_ptr<expression> m_item;

		public:
			explicit get_reference(std::unique_ptr<expression>&& item, source_location&& location)
				: ast_element(std::move(location)),
				m_item(std::move(item)) { }

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<get_reference>(m_item->clone(), source_location(location()));
			}
		};

		class arithmetic_operator final : public expression {
		private:
			std::unique_ptr<expression> m_right;
			std::unique_ptr<expression> m_left;
			token_type m_operation;

		public:
			const static std::map<token_type, int> operator_precedence;

			arithmetic_operator(token_type operation, std::unique_ptr<expression>&& left, std::unique_ptr<expression>&& right, source_location&& location)
				: ast_element(std::move(location)),
				m_right(std::move(right)),
				m_left(std::move(left)),
				m_operation(operation) {}

			token_type operation() const noexcept { return m_operation; }

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<arithmetic_operator>(m_operation, m_left->clone(), m_right->clone(), source_location(location()));
			}
		};

		class conditional_expression final : public expression {
		private:
			std::unique_ptr<expression> m_condition;
			std::unique_ptr<expression> m_true_expr;
			std::unique_ptr<expression> m_false_expr;

		public:
			conditional_expression(std::unique_ptr<expression>&& condition,
				std::unique_ptr<expression>&& true_expr,
				std::unique_ptr<expression>&& false_expr,
				source_location&& location)
				: ast_element(std::move(location)),
				m_condition(std::move(condition)),
				m_true_expr(std::move(true_expr)),
				m_false_expr(std::move(false_expr)) {}

			const expression* condition() const noexcept { return m_condition.get(); }
			const expression* true_expr() const noexcept { return m_true_expr.get(); }
			const expression* false_expr() const noexcept { return m_false_expr.get(); }

			void build_c_string_prec(std::stringstream&, int) const override;

			std::unique_ptr<expression> clone() const override {
				return std::make_unique<conditional_expression>(m_condition->clone(), m_true_expr->clone(), m_false_expr->clone(), source_location(location()));
			}
		};

		class function_call : public expression, public statement {
		private:
			std::unique_ptr<expression> m_callee;
			std::vector<std::unique_ptr<expression>> m_arguments;

		public:
			function_call(std::unique_ptr<expression> callee,
				std::vector<std::unique_ptr<expression>> arguments,
				source_location location)
				: ast_element(std::move(location)),
				m_callee(std::move(callee)),
				m_arguments(std::move(arguments)) {}

			void build_c_string_prec(std::stringstream&, int) const override;
			void build_c_string_indent(std::stringstream&, int) const override;

			void build_c_string(std::stringstream& ss) const override {
				build_c_string_indent(ss, 0);
			}

			std::unique_ptr<expression> clone() const override {
				std::vector<std::unique_ptr<expression>> cloned_arguments;
				cloned_arguments.reserve(m_arguments.size());
				for (const auto& arg : m_arguments) {
					cloned_arguments.push_back(arg->clone());
				}
				return std::make_unique<function_call>(m_callee->clone(), std::move(cloned_arguments), source_location(location()));
			}
		};

		class variable_declaration final : public statement, public top_level_element {
		private:
			uint8_t m_qualifiers;
			std::unique_ptr<type> m_type;
			std::string m_identifier;
			std::optional<std::unique_ptr<expression>> m_set_value;

		public:
			variable_declaration(uint8_t qualifiers,
				std::unique_ptr<type>&& var_type,
				const std::string& identifier, source_location&& location,
				std::optional<std::unique_ptr<expression>>&& set_value = std::nullopt)
				: ast_element(std::move(location)),
				m_qualifiers(qualifiers),
				m_type(std::move(var_type)),
				m_identifier(identifier),
				m_set_value(set_value ? std::make_optional<std::unique_ptr<expression>>(std::move(set_value.value())) : std::nullopt) {}

			uint8_t qualifiers() const noexcept { return m_qualifiers; }
			const type* type() const noexcept { return m_type.get(); }
			const std::string& identifier() const noexcept { return m_identifier; }
			const expression* set_value() const noexcept { return m_set_value.has_value() ? m_set_value.value().get() : nullptr; }

			void build_c_string_indent(std::stringstream&, int) const override;
			
			void build_c_string(std::stringstream& ss) const override {
				build_c_string_indent(ss, 0);
			}
		};

		class typedef_declaration final : public top_level_element {
		private:
			std::unique_ptr<type> m_type;
			std::string m_name;

		public:
			typedef_declaration(std::unique_ptr<type>&& type, std::string&& name, source_location&& location)
				: ast_element(std::move(location)),
				m_type(std::move(type)), m_name(std::move(name)) { }

			const type* type() const noexcept { return m_type.get(); }
			const std::string& name() const noexcept { return m_name; }

			void build_c_string(std::stringstream&) const override;
		};

		class struct_declaration final : public top_level_element, public type {
		private:
			std::optional<std::string> m_struct_name;
			std::vector<variable_declaration> m_members;

		public:
			struct_declaration(std::optional<std::string>&& struct_name, std::vector<variable_declaration>&& members, source_location&& location)
				: ast_element(std::move(location)),
				m_struct_name(std::move(struct_name)), m_members(std::move(members)) {}

			const std::optional<std::string> struct_name() const noexcept { return m_struct_name; }
			const std::vector<variable_declaration>& members() const noexcept { return m_members; }

			void build_c_string(std::stringstream&) const override;

			std::unique_ptr<type> clone() const override {
				std::vector<variable_declaration> cloned_members;
				cloned_members.reserve(m_members.size());
				for (const variable_declaration& member : m_members) {
					cloned_members.emplace_back(variable_declaration(member.qualifiers(), member.type()->clone(), std::string(member.identifier()), source_location(member.location()), member.set_value() != nullptr ? std::make_optional(member.set_value()->clone()) : std::nullopt));
				}
				return std::make_unique<struct_declaration>(std::optional<std::string>(m_struct_name), std::move(cloned_members), source_location(location()));
			}
		};

		class enum_declaration final : public top_level_element, public type {
		public:
			struct enumerator {
				std::string name;
				std::optional<int64_t> value;

				enumerator(std::string&& name, std::optional<int64_t> value = std::nullopt)
					: name(std::move(name)), value(value) {}
			};

		private:
			std::optional<std::string> m_enum_name;
			std::vector<enumerator> m_enumerators;

		public:
			enum_declaration(std::optional<std::string>&& enum_name, std::vector<enumerator>&& enumerators, source_location&& location)
				: ast_element(std::move(location)),
				m_enum_name(std::move(enum_name)), m_enumerators(std::move(enumerators)) {}

			const std::optional<std::string> enum_name() const noexcept { return m_enum_name; }
			const std::vector<enumerator>& enumerators() const noexcept { return m_enumerators; }

			void build_c_string(std::stringstream&) const override;

			std::unique_ptr<type> clone() const override {
				std::vector<enumerator> cloned_enumerators;
				cloned_enumerators.reserve(m_enumerators.size());
				for (const auto& e : m_enumerators) {
					cloned_enumerators.emplace_back(std::string(e.name), e.value);
				}
				return std::make_unique<enum_declaration>(std::optional<std::string>(m_enum_name), std::move(cloned_enumerators), source_location(location()));
			}
		};

		class union_declaration : public top_level_element, public type {
		public:
			struct member {
				std::unique_ptr<type> member_type;
				std::string member_name;

				member(std::unique_ptr<type>&& member_type, std::string&& member_name)
					: member_type(std::move(member_type)), member_name(std::move(member_name)) {}
			};

		private:
			std::optional<std::string> m_union_name;
			std::vector<member> m_members;

		public:
			union_declaration(std::optional<std::string>&& union_name, std::vector<member>&& members, source_location&& location)
				: ast_element(std::move(location)),
				m_union_name(std::move(union_name)), m_members(std::move(members)) {}

			const std::optional<std::string> union_name() const noexcept { return m_union_name; }
			const std::vector<member>& members() const noexcept { return m_members; }

			void build_c_string(std::stringstream&) const override;

			std::unique_ptr<type> clone() const override {
				std::vector<member> cloned_members;
				cloned_members.reserve(m_members.size());
				for (const auto& m : m_members) {
					cloned_members.emplace_back(m.member_type->clone(), std::string(m.member_name));
				}
				return std::make_unique<union_declaration>(std::optional<std::string>(m_union_name), std::move(cloned_members), source_location(location()));
			}
		};

		// Function representation
		struct function_parameter {
			uint8_t qualifiers;
			std::unique_ptr<type> param_type;
			std::string param_name;

			function_parameter(std::unique_ptr<type>&& param_type, std::string&& param_name, uint8_t qualifiers)
				: param_type(std::move(param_type)), param_name(std::move(param_name)), qualifiers(qualifiers) {}
		};

		class function_prototype : public top_level_element {
		private:
			std::unique_ptr<type> m_return_type;
			std::string m_function_name;
			std::vector<function_parameter> m_parameters;

		public:
			function_prototype(std::unique_ptr<type>&& return_type,
				std::string&& function_name,
				std::vector<function_parameter>&& parameters,
				source_location&& location)
				: ast_element(std::move(location)),
				m_return_type(std::move(return_type)),
				m_function_name(std::move(function_name)),
				m_parameters(std::move(parameters)) { }

			const type* return_type() const noexcept { return m_return_type.get(); }
			const std::string& function_name() const noexcept { return m_function_name; }
			const std::vector<function_parameter>& parameters() const noexcept { return m_parameters; }

			void build_c_string(std::stringstream&) const override;
		};

		class function_declaration : public top_level_element {
		private:
			std::unique_ptr<type> m_return_type;
			std::string m_function_name;
			std::vector<function_parameter> m_parameters;
			context_block m_function_body;

		public:
			function_declaration(std::unique_ptr<type>&& return_type,
				std::string&& function_name,
				std::vector<function_parameter>&& parameters,
				context_block&& function_body, 
				source_location&& location)
				: ast_element(std::move(location)),
				m_return_type(std::move(return_type)),
				m_function_name(std::move(function_name)),
				m_parameters(std::move(parameters)),
				m_function_body(std::move(function_body)) {}

			const type* return_type() const noexcept { return m_return_type.get(); }
			const std::string& function_name() const noexcept { return m_function_name; }
			const std::vector<function_parameter>& parameters() const noexcept { return m_parameters; }
			const context_block& function_body() const noexcept { return m_function_body; }

			void build_c_string(std::stringstream&) const override;
		};
	}
}