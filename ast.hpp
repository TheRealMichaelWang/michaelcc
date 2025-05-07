#pragma once

#include <map>
#include <memory>
#include <vector>
#include <optional>
#include <cstdint>

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
		};

		class template_argument {
		public:
		};

		class top_level_element : virtual public ast_element {
		public:
		};

		class statement : virtual public ast_element {
		public:
		};

		class expression : virtual public ast_element, public template_argument {
		public:
		};

		class set_destination : public expression {
		public:
		};

		class type : public template_argument {
		public:
		};

		class int_type : public type {
		private:
			uint8_t m_qualifiers;
			int_class m_class;

		public:
			int_type(uint8_t qualifiers, int_class m_class)
				: m_qualifiers(qualifiers), m_class(m_class) { }
		};

		class float_type : public type {
		private:
			float_class m_class;

		public:
			explicit float_type(float_class m_class) : m_class(m_class) { }
		};

		class pointer_type : public type {
		private:
			std::unique_ptr<type> m_pointee_type;
		public:
			pointer_type(std::unique_ptr<type>&& pointee_type)
				: m_pointee_type(std::move(pointee_type)) {}
			const type* pointee_type() const noexcept { return m_pointee_type.get(); }
		};

		// Array type, e.g. int[10], float[]
		class array_type : public type {
		private:
			std::unique_ptr<type> m_element_type;
			std::optional<std::unique_ptr<expression>> m_length;
		public:
			array_type(std::unique_ptr<type>&& element_type, std::optional<std::unique_ptr<expression>>&& length)
				: m_element_type(std::move(element_type)), m_length(std::move(length)) {}
			const type* element_type() const noexcept { return m_element_type.get(); }
			const std::optional<std::unique_ptr<expression>>& length() const noexcept { return m_length; }
		};

		class template_type final : public type, public ast_element {
		private:
			std::string m_identifier;
			std::vector<std::unique_ptr<template_argument>> m_template_arguments;

		public:
			explicit template_type(std::string&& identifier, std::vector<std::unique_ptr<template_argument>>&& template_arguments, source_location&& location)
				: ast_element(std::move(location)), 
				 m_identifier(std::move(identifier)),
				 m_template_arguments(std::move(template_arguments)) { }
		};

		class context_block : public statement {
		private:
			std::vector<std::unique_ptr<statement>> m_statements;

		public:
			explicit context_block(std::vector<std::unique_ptr<statement>>&& statements, source_location&& location)
				: ast_element(std::move(location)), m_statements(std::move(statements)) { }
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
		};

		class return_statement final : public statement {
		private:
			std::unique_ptr<expression> value;

		public:
			explicit return_statement(std::unique_ptr<expression>&& return_value, source_location&& location)
				: ast_element(std::move(location)),
				value(std::move(return_value)) { }
		};

		class break_statement final : public statement {
		private:
			int loop_depth;

		public:
			explicit break_statement(int depth, source_location&& location) 
				: ast_element(std::move(location)),
				loop_depth(depth) { }
		};

		class continue_statement final : public statement {
		private:
			int loop_depth;

		public:
			explicit continue_statement(int depth, source_location&& location)
				: ast_element(std::move(location)), loop_depth(depth) { }
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
		};

		class variable_reference final : public set_destination {
		private:
			std::string m_identifier;

		public:
			explicit variable_reference(const std::string& identifier, source_location&& location)
				: ast_element(std::move(location)),
				m_identifier(identifier) { }

			const std::string& identifier() const noexcept { return m_identifier; }
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
		};

		class dereference_operator final : public set_destination {
		private:
			std::unique_ptr<expression> m_pointer;

		public:
			explicit dereference_operator(std::unique_ptr<expression>&& pointer, source_location&& location)
				: ast_element(std::move(location)),
				m_pointer(std::move(pointer)) { }
		};

		class get_reference final : public expression {
		private:
			std::unique_ptr<expression> m_item;

		public:
			explicit get_reference(std::unique_ptr<expression>&& item, source_location&& location)
				: ast_element(std::move(location)),
				m_item(std::move(item)) { }
		};

		class arithmetic_operator final : public expression {
		private:
			std::unique_ptr<expression> m_right;
			std::unique_ptr<expression> m_left;
			token_type m_operation;

		public:
			arithmetic_operator(token_type operation, std::unique_ptr<expression>&& left, std::unique_ptr<expression>&& right, source_location&& location)
				: ast_element(std::move(location)), 
				m_right(std::move(right)),
				m_left(std::move(left)),
				m_operation(operation) {}

			token_type operation() const noexcept { return m_operation; }
		};

		class variable_declaration final : public statement {
		private:
			uint8_t m_qualifiers;
			std::unique_ptr<type> m_type;
			std::string m_identifier;
			std::optional<std::unique_ptr<expression>> m_set_value;
			std::optional<std::unique_ptr<expression>> m_array_length_value;

		public:
			variable_declaration(uint8_t qualifiers,
				std::unique_ptr<type>&& var_type,
				const std::string& identifier, source_location&& location,
				std::optional<std::unique_ptr<expression>>&& set_value = std::nullopt,
				std::optional<std::unique_ptr<expression>>&& array_length_value = std::nullopt)
				: ast_element(std::move(location)),
				m_qualifiers(qualifiers),
				m_type(std::move(var_type)),
				m_identifier(identifier),
				m_set_value(set_value ? std::make_optional<std::unique_ptr<expression>>(std::move(set_value.value())) : std::nullopt),
				m_array_length_value(array_length_value ? std::make_optional<std::unique_ptr<expression>>(std::move(array_length_value.value())) : std::nullopt) {}

			uint8_t qualifiers() const noexcept { return m_qualifiers; }
			const type* type() const noexcept { return m_type.get(); }
			const std::string& identifier() const noexcept { return m_identifier; }
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
		};

		// Struct representation
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
		};

		// Enum representation
		class enum_declaration final : public top_level_element, public type  {
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
		};

		// Union representation
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
		};
	}
}