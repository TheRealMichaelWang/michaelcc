#pragma once

#include <map>
#include <memory>
#include <vector>

namespace michaelcc::ast {
	class statement {
	public:
	};

	class rvalue {
	public:
	};

	class lvalue {
	public:
	};

	class type {
	public:
	};

	class context_block : public statement {
	private:
		std::vector<std::unique_ptr<statement>> m_statements;
	};

	class for_loop : public statement {
	private:
		std::unique_ptr<statement> m_initial_statement;
		std::unique_ptr<lvalue> m_condition;
		std::unique_ptr<statement> m_increment_statement;
		context_block to_execute;
	};

	class do_block : public statement {
	private:
		std::unique_ptr<lvalue> m_condition;
		context_block m_to_execute;
	};

	class while_block : public statement {
	private:
		std::unique_ptr<lvalue> m_condition;
		context_block m_to_execute;
	};

	class if_block : public statement {
		std::unique_ptr<lvalue> m_condition;
		context_block m_execute_if_true;
	};

	class if_else_block : public statement {
	private:
		std::unique_ptr<lvalue> m_condition;
		context_block m_execute_if_true;
		context_block m_execute_if_false;
	};
	
	class return_statement : public statement {
	private:
		std::unique_ptr<lvalue> value;
	};

	class break_statement : public statement {
	private:
		int loop_depth;
	};

	class continue_statement : public statement {
	private:
		int loop_depth;
	};

	class integer_literal : public lvalue {
	private:
		size_t value;
	};

	class variable_reference : public rvalue, public lvalue {
	private:
		std::string m_identifier;
	};

	class get_index : public rvalue, public lvalue {
	private:
		std::unique_ptr<lvalue> m_ptr;
		std::unique_ptr<lvalue> m_index;
	};

	class dereference_operator : public lvalue, public rvalue {
	private:
		std::unique_ptr<lvalue> m_pointer;
	};

	class set_operator : public lvalue {
	private:
		std::unique_ptr<rvalue> m_right;
		std::unique_ptr<lvalue> m_left;
	};

	class arithmetic_operator : public lvalue {
	private:
		std::unique_ptr<lvalue> m_right;
		std::unique_ptr<lvalue> m_left;
		token_type m_operation;

	public:
		const token_type operation() const noexcept {
			return m_operation;
		}
	};

	class variable_declaration : public statement {
	private:
		bool m_const_qualified;
		type m_type;
		std::string m_identifier;

		std::optional<std::unique_ptr<lvalue>> m_set_value;
	};
}