#ifndef MICHAELCC_LOGICAL_IR_HPP
#define MICHAELCC_LOGICAL_IR_HPP

#include <memory>
#include <string>
#include <cstdint>
#include "tokens.hpp"
#include "symbols.hpp"
#include "typing.hpp"
#include "utils.hpp"

namespace michaelcc {
	namespace logical_ir {
		class expression;
		class statement;
		class variable;
		class function_definition;
		class control_block;
		class integer_constant;
		class floating_constant;
		class string_constant;
		class variable_reference;
		class binary_operation;
		class unary_operation;
		class type_cast;
		class address_of;
		class dereference;
		class member_access;
		class array_index;
		class function_call;
		class conditional_expression;
		class expression_statement;
		class assignment_statement;
		class local_declaration;
		class return_statement;
		class if_statement;
		class loop_statement;
		class break_statement;
		class continue_statement;
		class translation_unit;

		using visitor = generic_visitor<
			variable,
			control_block,
			function_definition,
			integer_constant,
			floating_constant,
			string_constant,
			variable_reference,
			binary_operation,
			unary_operation,
			type_cast,
			address_of,
			dereference,
			member_access,
			array_index,
			function_call,
			conditional_expression,
			expression_statement,
			assignment_statement,
			local_declaration,
			return_statement,
			if_statement,
			loop_statement,
			break_statement,
			continue_statement,
			translation_unit
		>;

		class variable final : public symbol, public visitable_base<visitor> {
		private:
            uint8_t m_qualifiers;
            typing::qual_type m_type;
			bool m_is_global;
		public:
			variable(std::string&& name, typing::qual_type&& var_type, bool is_global, std::weak_ptr<symbol_context>&& context)
				: symbol(std::move(name), std::move(context)), m_type(std::move(var_type)), m_is_global(is_global) {}

			const typing::qual_type& get_type() const noexcept { return m_type; }
			bool is_global() const noexcept { return m_is_global; }

            std::string to_string() const noexcept override {
                return std::format("{} variable {}", m_is_global ? "global" : "local", name());
            }

			void accept(visitor& v) const override { v.visit(*this); }
		};

		class expression : public visitable_base<visitor> {
		public:
			virtual ~expression() = default;
			virtual const typing::qual_type get_type() const = 0;
			virtual void accept(visitor& v) const override = 0;
		};

		class integer_constant final : public expression {
		private:
			uint64_t m_value;
			typing::qual_type m_type;
		public:
			integer_constant(uint64_t value, typing::qual_type&& int_type)
				: m_value(value), m_type(std::move(int_type)) {}

			uint64_t value() const noexcept { return m_value; }
			
            const typing::qual_type get_type() const override { 
                return m_type; 
            }

			void accept(visitor& v) const override { v.visit(*this); }
		};

		class floating_constant final : public expression {
		private:
			double m_value;
			typing::qual_type m_type;
		public:
			floating_constant(double value, typing::qual_type&& float_type)
				: m_value(value), m_type(std::move(float_type)) {}

			double value() const noexcept { return m_value; }

			const typing::qual_type get_type() const override { 
                return m_type; 
            }

			void accept(visitor& v) const override { v.visit(*this); }
		};

		class string_constant final : public expression {
		private:
			size_t m_index;
		public:
			explicit string_constant(size_t index) : m_index(index) {}

			size_t index() const noexcept { return m_index; }
			const typing::qual_type get_type() const override { 
                return typing::qual_type::owning(std::make_shared<typing::pointer_type>(
                    typing::qual_type::owning(
                        std::make_shared<typing::int_type>(typing::NO_INT_QUALIFIER, typing::CHAR_INT_CLASS)
                    )
                ));     
            }

			void accept(visitor& v) const override { v.visit(*this); }
		};

		class variable_reference final : public expression {
		private:
			std::shared_ptr<variable> m_variable;
		public:
			explicit variable_reference(std::shared_ptr<variable>&& var) : m_variable(std::move(var)) {}

			const std::shared_ptr<variable>& get_variable() const noexcept { return m_variable; }
			bool is_global() const noexcept { return m_variable->is_global(); }

			const typing::qual_type get_type() const override {
				return m_variable->get_type();
			}

			void accept(visitor& v) const override { v.visit(*this); }
		};

		class binary_operation final : public expression {
		private:
			token_type m_operator;
			std::unique_ptr<expression> m_left;
			std::unique_ptr<expression> m_right;

            typing::qual_type m_result_type;
		public:
			binary_operation(token_type op,
				std::unique_ptr<expression>&& left,
				std::unique_ptr<expression>&& right,
				typing::qual_type&& result_type)
				: m_operator(op),
				m_left(std::move(left)),
				m_right(std::move(right)),
				m_result_type(std::move(result_type)) {}

			token_type get_operator() const noexcept { return m_operator; }
			const expression* left() const noexcept { return m_left.get(); }
			const expression* right() const noexcept { return m_right.get(); }
			
            const typing::qual_type get_type() const override { return m_result_type; }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_left->accept(v);
				m_right->accept(v);
			}
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

            const typing::qual_type get_type() const override { return m_operand->get_type(); }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_operand->accept(v);
			}
		};

		class type_cast final : public expression {
		private:
			std::unique_ptr<expression> m_operand;
			typing::qual_type m_target_type;
		public:
			type_cast(std::unique_ptr<expression>&& operand, typing::qual_type&& target_type)
				: m_operand(std::move(operand)), m_target_type(std::move(target_type)) {}

			const expression* operand() const noexcept { return m_operand.get(); }
			const typing::qual_type get_type() const override { return m_target_type; }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_operand->accept(v);
			}
		};

		class address_of final : public expression {
		private:
			std::unique_ptr<expression> m_operand;
		public:
			address_of(std::unique_ptr<expression>&& operand)
				: m_operand(std::move(operand)) {}

			const expression* operand() const noexcept { return m_operand.get(); }

			const typing::qual_type get_type() const override { 
                return typing::qual_type(std::make_shared<typing::pointer_type>(m_operand->get_type().to_weak()));
            }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_operand->accept(v);
			}
		};

		class dereference final : public expression {
		private:
			std::unique_ptr<expression> m_operand;
		public:
			explicit dereference(std::unique_ptr<expression>&& operand) : m_operand(std::move(operand)) {}

			const expression* operand() const noexcept { return m_operand.get(); }

			const typing::qual_type get_type() const override {
                auto operand_type = m_operand->get_type();
				auto* ptr_type = dynamic_cast<const typing::pointer_type*>(operand_type.type().get());
                if (ptr_type == nullptr) {
                    throw std::runtime_error("Operand is not a pointer");
                }
				return ptr_type->pointee_type().to_owning();
			}

			void accept(visitor& v) const override {
				v.visit(*this);
				m_operand->accept(v);
			}
		};

		class member_access final : public expression {
		private:
			std::unique_ptr<expression> m_base;
			size_t m_field_index;
			typing::qual_type m_field_type; 
		public:
			member_access(std::unique_ptr<expression>&& base, size_t field_index, typing::qual_type&& field_type)
				: m_base(std::move(base)), m_field_index(field_index), m_field_type(std::move(field_type)) {}

			const expression* base() const noexcept { return m_base.get(); }
			size_t field_index() const noexcept { return m_field_index; }
			const typing::qual_type get_type() const override { return m_field_type; }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_base->accept(v);
			}
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

			const typing::qual_type get_type() const override {
				auto base_type = m_base->get_type();
				auto* arr_type = dynamic_cast<const typing::array_type*>(base_type.type().get());
				if (arr_type == nullptr) {
                    throw std::runtime_error("Base is not an array");
                }
				return arr_type->element_type().to_owning();
            }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_base->accept(v);
				m_index->accept(v);
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

			const typing::qual_type get_type() const override {
				auto callee_type = m_callee->get_type();
				auto* fn_type = dynamic_cast<const typing::function_pointer_type*>(callee_type.type().get());
				if (fn_type == nullptr) {
                    throw std::runtime_error("Callee is not a function pointer");
                }
				return fn_type->return_type().to_owning();
			}

			void accept(visitor& v) const override {
				v.visit(*this);
				m_callee->accept(v);
				for (const auto& arg : m_arguments) {
					arg->accept(v);
				}
			}
		};

		class conditional_expression final : public expression {
		private:
			std::unique_ptr<expression> m_condition;
			std::unique_ptr<expression> m_then_expression;
			std::unique_ptr<expression> m_else_expression;
			typing::qual_type m_result_type;
		public:
			conditional_expression(std::unique_ptr<expression>&& condition,
				std::unique_ptr<expression>&& then_expression,
				std::unique_ptr<expression>&& else_expression,
				typing::qual_type&& result_type)
				: m_condition(std::move(condition)),
				m_then_expression(std::move(then_expression)),
				m_else_expression(std::move(else_expression)),
				m_result_type(std::move(result_type)) {}

			const expression* condition() const noexcept { return m_condition.get(); }
			const expression* then_expression() const noexcept { return m_then_expression.get(); }
			const expression* else_expression() const noexcept { return m_else_expression.get(); }
			
            const typing::qual_type get_type() const override { 
                return m_result_type; 
            }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_condition->accept(v);
				m_then_expression->accept(v);
				m_else_expression->accept(v);
			}
		};

		class statement : public visitable_base<visitor> {
		public:
			virtual ~statement() = default;
			virtual void accept(visitor& v) const override = 0;
		};

		class control_block final : public symbol_context, public visitable_base<visitor> {
		private:
			std::vector<std::unique_ptr<statement>> m_statements;

		public:
			control_block(std::vector<std::unique_ptr<statement>>&& statements, std::weak_ptr<symbol_context>&& parent_context)
				: symbol_context(std::move(parent_context)), m_statements(std::move(statements)) {}

			const std::vector<std::unique_ptr<statement>>& statements() const noexcept { return m_statements; }

			void accept(visitor& v) const override {
				v.visit(*this);
				for (const auto& stmt : m_statements) {
					stmt->accept(v);
				}
			}
		};

		class expression_statement final : public statement {
		private:
			std::unique_ptr<expression> m_expression;
		public:
			explicit expression_statement(std::unique_ptr<expression>&& expr)
				: m_expression(std::move(expr)) {}

			const expression* get_expression() const noexcept { return m_expression.get(); }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_expression->accept(v);
			}
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

			void accept(visitor& v) const override {
				v.visit(*this);
				m_destination->accept(v);
				m_value->accept(v);
			}
		};

		class local_declaration final : public statement {
		private:
			std::shared_ptr<variable> m_variable;
			std::unique_ptr<expression> m_initializer;
		public:
			local_declaration(std::shared_ptr<variable>&& var, std::unique_ptr<expression>&& initializer = nullptr)
				: m_variable(std::move(var)), m_initializer(std::move(initializer)) {}

			const std::shared_ptr<variable>& get_variable() const noexcept { return m_variable; }
			const expression* initializer() const noexcept { return m_initializer.get(); }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_variable->accept(v);
				if (m_initializer) {
					m_initializer->accept(v);
				}
			}
		};

		class return_statement final : public statement {
		private:
			std::unique_ptr<expression> m_value;
		public:
			explicit return_statement(std::unique_ptr<expression>&& value = nullptr)
				: m_value(std::move(value)) {}

			const expression* value() const noexcept { return m_value.get(); }

			void accept(visitor& v) const override {
				v.visit(*this);
				if (m_value) {
					m_value->accept(v);
				}
			}
		};

		class if_statement final : public statement {
		private:
			std::unique_ptr<expression> m_condition;
			std::shared_ptr<control_block> m_then_body;
			std::shared_ptr<control_block> m_else_body;
		public:
			if_statement(std::unique_ptr<expression>&& condition,
				std::shared_ptr<control_block>&& then_body,
				std::shared_ptr<control_block>&& else_body)
				: m_condition(std::move(condition)),
				m_then_body(std::move(then_body)),
				m_else_body(std::move(else_body)) {}

			const expression* condition() const noexcept { return m_condition.get(); }
			const std::shared_ptr<control_block>& then_body() const noexcept { return m_then_body; }
			const std::shared_ptr<control_block>& else_body() const noexcept { return m_else_body; }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_condition->accept(v);
				m_then_body->accept(v);
				if (m_else_body) {
					m_else_body->accept(v);
				}
			}
		};

		class loop_statement final : public statement {
		private:
			std::shared_ptr<control_block> m_body;
			std::unique_ptr<expression> m_condition;
			bool m_check_condition_first;
		public:
			loop_statement(std::shared_ptr<control_block>&& body,
				std::unique_ptr<expression>&& condition,
				bool check_condition_first = true)
				: m_body(std::move(body)),
				m_condition(std::move(condition)),
				m_check_condition_first(check_condition_first) {}

			const std::shared_ptr<control_block>& body() const noexcept { return m_body; }
			const expression* condition() const noexcept { return m_condition.get(); }
			bool check_condition_first() const noexcept { return m_check_condition_first; }

			void accept(visitor& v) const override {
				v.visit(*this);
				m_condition->accept(v);
				m_body->accept(v);
			}
		};

		class break_statement final : public statement {
		public:
			void accept(visitor& v) const override { v.visit(*this); }
		};

		class continue_statement final : public statement {
		public:
			void accept(visitor& v) const override { v.visit(*this); }
		};

		class function_definition final : public symbol, public visitable_base<visitor> {
		private:
			std::vector<std::shared_ptr<variable>> m_parameters;
			std::shared_ptr<control_block> m_body;

		public:
			explicit function_definition(std::string&& name, std::vector<std::shared_ptr<variable>>&& parameters, std::weak_ptr<symbol_context>&& context)
				: symbol(std::move(name), std::move(context)), m_parameters(std::move(parameters)), m_body(nullptr) {}

			const std::vector<std::shared_ptr<variable>>& parameters() const noexcept { return m_parameters; }
			const std::shared_ptr<control_block>& body() const { return m_body; }

			bool is_implemented() const noexcept { return m_body != nullptr; }

            void implement_body(std::shared_ptr<control_block>&& body) {
                if (is_implemented()) {
                    throw std::runtime_error("Function already implemented");
                }

                //declare each parameter in the body individually
                m_body = std::move(body);
                for (const auto& parameter : m_parameters) {
                    m_body->add(std::shared_ptr<symbol>(parameter));
                    parameter->set_context(std::weak_ptr<symbol_context>(m_body));
                }
            }

            std::string to_string() const noexcept override {
                return std::format("function {}", name());
            }

			void accept(visitor& v) const override {
				v.visit(*this);
				for (const auto& param : m_parameters) {
					param->accept(v);
				}
				if (m_body) {
					m_body->accept(v);
				}
			}
		};

		class translation_unit {
		private:
            std::shared_ptr<symbol_context> m_global_context;
			std::vector<std::string> m_strings;

            std::unordered_map<std::string, std::shared_ptr<typing::struct_type>> m_structs;
            std::unordered_map<std::string, std::shared_ptr<typing::union_type>> m_unions;
            std::unordered_map<std::string, std::shared_ptr<typing::enum_type>> m_enums;
		public:
            translation_unit() : m_global_context(
                std::make_shared<symbol_context>(std::weak_ptr<symbol_context>())
            ) {}

            void declare_struct(std::unique_ptr<typing::struct_type>&& struct_type) {
                m_structs[struct_type->name().value()] = std::shared_ptr<typing::struct_type>(std::move(struct_type));
            }

            void declare_union(std::unique_ptr<typing::union_type>&& union_type) {
                m_unions[union_type->name().value()] = std::shared_ptr<typing::union_type>(std::move(union_type));
            }

            void declare_enum(std::unique_ptr<typing::enum_type>&& enum_type) {
                m_enums[enum_type->name().value()] = std::shared_ptr<typing::enum_type>(std::move(enum_type));
            }

            std::shared_ptr<typing::struct_type> lookup_struct(const std::string& name) {
                auto it = m_structs.find(name);
                return it != m_structs.end() ? it->second : nullptr;
            }

            std::shared_ptr<typing::union_type> lookup_union(const std::string& name) {
                auto it = m_unions.find(name);
                return it != m_unions.end() ? it->second : nullptr;
            }

            std::shared_ptr<typing::enum_type> lookup_enum(const std::string& name) {
                auto it = m_enums.find(name);
                return it != m_enums.end() ? it->second : nullptr;
            }

            std::shared_ptr<symbol> lookup_global(const std::string& name) {
                return m_global_context->lookup(name);
            }

            bool declare_global(std::shared_ptr<symbol>&& sym) {
                return m_global_context->add(std::move(sym));
            }

			size_t add_string(std::string&& str) {
				m_strings.push_back(std::move(str));
				return m_strings.size() - 1;
			}

			const std::string& get_string(size_t index) const { return m_strings[index]; }
			const std::vector<std::string>& strings() const noexcept { return m_strings; }

			const std::vector<std::shared_ptr<symbol>>& global_symbols() const noexcept { 
                return m_global_context->symbols();
            }

            const std::unordered_map<std::string, std::shared_ptr<typing::struct_type>>& structs() const noexcept { return m_structs; }
            const std::unordered_map<std::string, std::shared_ptr<typing::union_type>>& unions() const noexcept { return m_unions; }
            const std::unordered_map<std::string, std::shared_ptr<typing::enum_type>>& enums() const noexcept { return m_enums; }

            const std::shared_ptr<symbol_context>& global_context() const noexcept { return m_global_context; }

			void accept(visitor& v) const {
				v.visit(*this);
				for (const auto& sym : m_global_context->symbols()) {
					auto* var = dynamic_cast<variable*>(sym.get());
					if (var) {
						var->accept(v);
						continue;
					}
					auto* func = dynamic_cast<function_definition*>(sym.get());
					if (func) {
						func->accept(v);
					}
				}
			}
		};

		// Utility function to print the IR as a tree
		std::string to_tree_string(const translation_unit& unit);
	}
}

#endif
