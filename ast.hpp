#pragma once

#include <map>
#include <memory>
#include <vector>

namespace michaelcc::ast {
	enum storage_qualifier : char {
		NO_STORAGE_QUALIFIER = 0,
		CONST_STORAGE_QUALIFIER = 1,
		VOLATILE_STORAGE_QUALIFIER = 2,
		RESTRICT_STORAGE_QUALIFIER = 4,
		EXTERN_STORAGE_QUALIFIER = 8,
		STATIC_STORAGE_QUALIFIER = 16,
		REGISTER_STORAGE_QUALIFIER = 32
	};

	enum int_qualifier : char {
		NO_INT_QUALIFIER = 0,
		LONG_INT_QUALIFIER = 1,
		SHORT_INT_QUALIFIER = 2,
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

	class top_level_element {
	public:
		virtual ~top_level_element() = default;
	};

	class statement {
	public:
		virtual ~statement() = default;
	};

	class rvalue {
	public:
		virtual ~rvalue() = default;
	};

	class lvalue {
	public:
		virtual ~lvalue() = default;
	};

	class type {
	public:
		virtual ~type() = default;

		virtual bool check_type(std::unique_ptr<type>& type) = 0;
	};

	class context_block : public statement {
	private:
		std::vector<std::unique_ptr<statement>> m_statements;

	public:
		explicit context_block(std::vector<std::unique_ptr<statement>> statements)
			: m_statements(std::move(statements)) { }
	};

	class for_loop : public statement {
	private:
		std::unique_ptr<statement> m_initial_statement;
		std::unique_ptr<lvalue> m_condition;
		std::unique_ptr<statement> m_increment_statement;
		context_block m_to_execute;

	public:
		for_loop(std::unique_ptr<statement> initial_statement,
			std::unique_ptr<lvalue> condition,
			std::unique_ptr<statement> increment_statement,
			context_block to_execute)
			: m_initial_statement(std::move(initial_statement)),
			m_condition(std::move(condition)),
			m_increment_statement(std::move(increment_statement)),
			m_to_execute(std::move(to_execute)) { }
	};

	class do_block : public statement {
	private:
		std::unique_ptr<lvalue> m_condition;
		context_block m_to_execute;

	public:
		do_block(std::unique_ptr<lvalue> condition, context_block to_execute)
			: m_condition(std::move(condition)),
			m_to_execute(std::move(to_execute)) { }
	};

	class while_block : public statement {
	private:
		std::unique_ptr<lvalue> m_condition;
		context_block m_to_execute;

	public:
		while_block(std::unique_ptr<lvalue> condition, context_block to_execute)
			: m_condition(std::move(condition)),
			m_to_execute(std::move(to_execute)) { }
	};

	class if_block : public statement {
	private:
		std::unique_ptr<lvalue> m_condition;
		context_block m_execute_if_true;

	public:
		if_block(std::unique_ptr<lvalue> condition, context_block execute_if_true)
			: m_condition(std::move(condition)),
			m_execute_if_true(std::move(execute_if_true)) { }
	};

	class if_else_block : public statement {
	private:
		std::unique_ptr<lvalue> m_condition;
		context_block m_execute_if_true;
		context_block m_execute_if_false;

	public:
		if_else_block(std::unique_ptr<lvalue> condition,
			context_block execute_if_true,
			context_block execute_if_false)
			: m_condition(std::move(condition)),
			m_execute_if_true(std::move(execute_if_true)),
			m_execute_if_false(std::move(execute_if_false)) { }
	};

	class return_statement : public statement {
	private:
		std::unique_ptr<lvalue> value;

	public:
		explicit return_statement(std::unique_ptr<lvalue> return_value)
			: value(std::move(return_value)) { }
	};

	class break_statement : public statement {
	private:
		int loop_depth;

	public:
		explicit break_statement(int depth) : loop_depth(depth) { }
	};

	class continue_statement : public statement {
	private:
		int loop_depth;

	public:
		explicit continue_statement(int depth) : loop_depth(depth) { }
	};

	class int_type : public type {
	private:
		int_qualifier m_qualifiers;
		int_class m_class;

	public:
		int_type(int_qualifier qualifiers, int_class m_class) 
			: m_qualifiers(qualifiers), m_class(m_class) { }
	};

	class float_type : public type {
	private:
		float_class m_class;

	public:
		explicit float_type(float_class m_class) : m_class(m_class) { }
	};

	class int_literal : public lvalue {
	private:
		int_qualifier m_qualifiers;
		int_class m_class;
		size_t m_value;
		
	public:
		explicit int_literal(int_qualifier qualifiers, int_class m_class, size_t value) 
			: m_qualifiers(qualifiers), m_class(m_class), m_value(value) { }

		const int_qualifier qualifiers() const noexcept {
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

	class float_literal : public lvalue {
	private:
		float_class m_class;
		double m_value;

	public:
		explicit float_literal(float_class m_class, double value) 
			: m_class(m_class), m_value(value) { }

		const float_class storage_class() const noexcept {
			return m_class;
		}

		const double value() const noexcept {
			return m_value;
		}
	};

	class variable_reference : public rvalue, public lvalue {
	private:
		std::string m_identifier;

	public:
		explicit variable_reference(const std::string& identifier)
			: m_identifier(identifier) { }

		const std::string& identifier() const noexcept { return m_identifier; }
	};

	class get_index : public rvalue, public lvalue {
	private:
		std::unique_ptr<lvalue> m_ptr;
		std::unique_ptr<lvalue> m_index;

	public:
		get_index(std::unique_ptr<lvalue> ptr, std::unique_ptr<lvalue> index)
			: m_ptr(std::move(ptr)),
			m_index(std::move(index)) { }
	};

	class dereference_operator : public lvalue, public rvalue {
	private:
		std::unique_ptr<lvalue> m_pointer;

	public:
		explicit dereference_operator(std::unique_ptr<lvalue> pointer)
			: m_pointer(std::move(pointer)) { }
	};

	class get_reference : public lvalue, public rvalue {
	private:
		std::unique_ptr<rvalue> m_item;

	public:
		explicit get_reference(std::unique_ptr<rvalue> item)
			: m_item(std::move(item)) { }
	};

	class set_operator : public lvalue {
	private:
		std::unique_ptr<rvalue> m_right;
		std::unique_ptr<lvalue> m_left;

	public:
		set_operator(std::unique_ptr<lvalue> left, std::unique_ptr<rvalue> right)
			: m_right(std::move(right)),
			m_left(std::move(left)) { }
	};

	class arithmetic_operator : public lvalue {
	private:
		std::unique_ptr<lvalue> m_right;
		std::unique_ptr<lvalue> m_left;
		token_type m_operation;

	public:
		arithmetic_operator(token_type operation,
			std::unique_ptr<lvalue> left,
			std::unique_ptr<lvalue> right)
			: m_right(std::move(right)),
			m_left(std::move(left)),
			m_operation(operation) {}

		token_type operation() const noexcept { return m_operation; }
	};

	class variable_declaration : public statement {
	private:
		storage_qualifier m_qualifiers;
		std::unique_ptr<type> m_type;
		std::string m_identifier;
		std::optional<std::unique_ptr<lvalue>> m_set_value;
		std::optional<std::unique_ptr<lvalue>> m_array_length_value;

	public:
		variable_declaration(storage_qualifier qualifiers,
			std::unique_ptr<type> var_type,
			const std::string& identifier,
			std::optional<std::unique_ptr<lvalue>> set_value = std::nullopt)
			: m_qualifiers(qualifiers),
			m_type(std::move(var_type)),
			m_identifier(identifier),
			m_set_value(set_value ? std::make_optional<std::unique_ptr<lvalue>>(std::move(set_value.value())) : std::nullopt) {}

		storage_qualifier qualifiers() const noexcept { return m_qualifiers; }
		const type* type() const noexcept { return m_type.get(); }
		const std::string& identifier() const noexcept { return m_identifier; }
	};

	class typedef_declaration : public top_level_element {
	private:
		std::unique_ptr<type> m_type;
		std::string m_name;

	public:
		typedef_declaration(std::unique_ptr<type>&& type, std::string name)
			: m_type(std::move(type)), m_name(name) { }

		const type* type() const noexcept { return m_type.get(); }
		const std::string& name() const noexcept { return m_name; }
	};

	// Struct representation
	class struct_declaration : public top_level_element, public type {
	private:
		std::optional<std::string> m_struct_name;
		std::vector<variable_declaration> m_members;

	public:
		struct_declaration(std::optional<std::string> struct_name, std::vector<variable_declaration> members)
			: m_struct_name(std::move(struct_name)), m_members(std::move(members)) {}

		const std::optional<std::string> struct_name() const noexcept { return m_struct_name; }
		const std::vector<variable_declaration>& members() const noexcept { return m_members; }
	};

	// Enum representation
	class enum_declaration : public top_level_element, public type {
	public:
		struct enumerator {
			std::string name;
			std::optional<int> value;

			enumerator(std::string name, std::optional<int> value = std::nullopt)
				: name(std::move(name)), value(value) {}
		};

	private:
		std::optional<std::string> m_enum_name;
		std::vector<enumerator> m_enumerators;

	public:
		enum_declaration(std::string enum_name, std::vector<enumerator> enumerators)
			: m_enum_name(std::move(enum_name)), m_enumerators(std::move(enumerators)) {}

		const std::optional<std::string> enum_name() const noexcept { return m_enum_name; }
		const std::vector<enumerator>& enumerators() const noexcept { return m_enumerators; }
	};

	// Union representation
	class union_declaration : public top_level_element, public type {
	public:
		struct member {
			std::unique_ptr<type> member_type;
			std::string member_name;

			member(std::unique_ptr<type>&& member_type, std::string member_name)
				: member_type(std::move(member_type)), member_name(std::move(member_name)) {}
		};

	private:
		std::optional<std::string> m_union_name;
		std::vector<member> m_members;

	public:
		union_declaration(std::string union_name, std::vector<member> members)
			: m_union_name(std::move(union_name)), m_members(std::move(members)) {}

		const std::optional<std::string> union_name() const noexcept { return m_union_name; }
		const std::vector<member>& members() const noexcept { return m_members; }
	};

	// Function representation
	struct function_parameter {
		storage_qualifier qualifiers;
		std::unique_ptr<type> param_type;
		std::string param_name;

		function_parameter(std::unique_ptr<type>&& param_type, std::string param_name, storage_qualifier qualifiers)
			: param_type(std::move(param_type)), param_name(std::move(param_name)), qualifiers(qualifiers) {}
	};

	class function_prototype : public top_level_element {
	private:
		std::unique_ptr<type> m_return_type;
		std::string m_function_name;
		std::vector<function_parameter> m_parameters;

	public:
		function_prototype(std::unique_ptr<type>&& return_type,
			std::string function_name,
			std::vector<function_parameter> parameters) 
			: m_return_type(std::move(return_type)),
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
			std::string function_name,
			std::vector<function_parameter> parameters,
			context_block function_body)
			: m_return_type(std::move(return_type)),
			m_function_name(std::move(function_name)),
			m_parameters(std::move(parameters)),
			m_function_body(std::move(function_body)) {}

		const type* return_type() const noexcept { return m_return_type.get(); }
		const std::string& function_name() const noexcept { return m_function_name; }
		const std::vector<function_parameter>& parameters() const noexcept { return m_parameters; }
		const context_block& function_body() const noexcept { return m_function_body; }
	};
}