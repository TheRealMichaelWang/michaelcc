#ifndef MICHAELCC_LOGICAL_IR_HPP
#define MICHAELCC_LOGICAL_IR_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <cstdint>
#include <variant>
#include "errors.hpp"
#include "syntax/tokens.hpp"
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
		class statement_block;
		class integer_constant;
		class floating_constant;
		class string_constant;
		class variable_reference;
		class function_reference;
		class increment_operator;
		class arithmetic_operator;
		class unary_operation;
		class type_cast;
		class address_of;
		class dereference;
		class member_access;
		class array_index;
		class array_initializer;
		class allocate_array;
		class struct_initializer;
		class union_initializer;
		class function_call;
		class conditional_expression;
		class set_address;
		class set_variable;
		class compound_expression;
		class expression_statement;
		class variable_declaration;
		class return_statement;
		class if_statement;
		class loop_statement;
		class break_statement;
		class continue_statement;
		class enumerator_literal;
		class translation_unit;

		class visitor : public mutable_generic_visitor<
			variable,
			control_block,
			statement_block,
			function_definition,
			integer_constant,
			floating_constant,
			string_constant,
			variable_reference,
			function_reference,
			increment_operator,
			arithmetic_operator,
			unary_operation,
			type_cast,
			address_of,
			dereference,
			member_access,
			array_index,
			array_initializer,
			struct_initializer,
			allocate_array,
			union_initializer,
			function_call,
			conditional_expression,
			set_address,
			set_variable,
			compound_expression,
			expression_statement,
			variable_declaration,
			return_statement,
			if_statement,
			loop_statement,
			break_statement,
			continue_statement,
			enumerator_literal,
			translation_unit
		> { 
		private:
			symbol_explorer m_explorer;

		public:
			symbol_explorer& explorer() noexcept { return m_explorer; }
		};

		using const_visitor = const_generic_visitor<
			variable,
			control_block,
			statement_block,
			function_definition,
			integer_constant,
			floating_constant,
			string_constant,
			variable_reference,
			function_reference,
			increment_operator,
			arithmetic_operator,
			unary_operation,
			type_cast,
			address_of,
			dereference,
			member_access,
			array_index,
			array_initializer,
			allocate_array,
			struct_initializer,
			union_initializer,
			function_call,
			conditional_expression,
			set_address,
			set_variable,
			compound_expression,
			expression_statement,
			variable_declaration,
			return_statement,
			if_statement,
			loop_statement,
			break_statement,
			continue_statement,
			enumerator_literal,
			translation_unit
		>;

		template<typename ReturnType>
		using symbol_dispatcher = generic_dispatcher<ReturnType, symbol, 
			variable,
			function_definition
		>;

		template<typename ReturnType>
		using const_symbol_dispatcher = generic_dispatcher<ReturnType, const symbol, 
			const variable,
			const function_definition
		>;

		template<typename ReturnType>
		using expression_dispatcher = generic_dispatcher<ReturnType, expression,
			integer_constant,
			floating_constant,
			string_constant,
			variable_reference,
			function_reference,
			increment_operator,
			arithmetic_operator,
			unary_operation,
			type_cast,
			address_of,
			dereference,
			member_access,
			array_index,
			array_initializer,
			allocate_array,
			struct_initializer,
			union_initializer,
			function_call,
			conditional_expression,
			compound_expression,
			set_address,
			set_variable,
			enumerator_literal
		>;

		template<typename ReturnType>
		using const_expression_dispatcher = generic_dispatcher<ReturnType, const expression,
			const integer_constant,
			const floating_constant,
			const string_constant,
			const variable_reference,
			const function_reference,
			const increment_operator,
			const arithmetic_operator,
			const unary_operation,
			const type_cast,
			const address_of,
			const dereference,
			const member_access,
			const array_index,
			const array_initializer,
			const allocate_array,
			const struct_initializer,
			const union_initializer,
			const function_call,
			const conditional_expression,
			const compound_expression,
			const set_address,
			const set_variable,
			const enumerator_literal
		>;
		template<typename ReturnType>
		using statement_dispatcher = generic_dispatcher<ReturnType, statement,
			expression_statement,
			variable_declaration,
			return_statement,
			if_statement,
			loop_statement,
			break_statement,
			continue_statement,
			statement_block
		>;

		template<typename ReturnType>
		using const_statement_dispatcher = generic_dispatcher<ReturnType, const statement,
			const expression_statement,
			const variable_declaration,
			const return_statement,
			const if_statement,
			const loop_statement,
			const break_statement,
			const continue_statement,
			const statement_block
		>;

		using expression_transformer = logical_ir::const_expression_dispatcher<std::unique_ptr<logical_ir::expression>>;
        using statement_transformer = logical_ir::const_statement_dispatcher<std::unique_ptr<logical_ir::statement>>;

		class variable final : public symbol, public mutable_visitable_base<visitor>, public const_visitable_base<const_visitor> {
		private:
            uint8_t m_qualifiers;
            typing::qual_type m_type;
			bool m_is_global;
		public:
			variable(std::string&& name, uint8_t qualifiers, typing::qual_type&& var_type, bool is_global, std::weak_ptr<symbol_context>&& context)
				: symbol(std::move(name), std::move(context)), m_qualifiers(qualifiers), m_type(std::move(var_type)), m_is_global(is_global) {}

			uint8_t qualifiers() const noexcept { return m_qualifiers; }
			const typing::qual_type& get_type() const noexcept { return m_type; }
			bool is_global() const noexcept { return m_is_global; }

            std::string to_string() const noexcept override {
                return std::format("{} variable {}", m_is_global ? "global" : "local", name());
            }

			void mutable_accept(visitor& v) override { v.visit(*this); }
			void accept(const_visitor& v) const override { v.visit(*this); }
		};

		class expression : public mutable_visitable_base<visitor>, public const_visitable_base<const_visitor> {
		public:
			virtual ~expression() = default;
			virtual const typing::qual_type get_type() const = 0;
			virtual void mutable_accept(visitor& v) override = 0;
			virtual void accept(const_visitor& v) const override = 0;
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

			void mutable_accept(visitor& v) override { v.visit(*this); }
			void accept(const_visitor& v) const override { v.visit(*this); }
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

			void mutable_accept(visitor& v) override { v.visit(*this); }
			void accept(const_visitor& v) const override { v.visit(*this); }
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

			void mutable_accept(visitor& v) override { v.visit(*this); }
			void accept(const_visitor& v) const override { v.visit(*this); }
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

			void mutable_accept(visitor& v) override { v.visit(*this); }
			void accept(const_visitor& v) const override { v.visit(*this); }
		};

		class increment_operator final : public expression {
		public:
			using destination_type = std::variant<std::unique_ptr<expression>, std::shared_ptr<variable>>;
		private:
			destination_type m_destination;
			std::optional<std::unique_ptr<expression>> m_increment_amount;

		public:
			increment_operator(destination_type&& destination, std::optional<std::unique_ptr<expression>>&& increment_amount = std::nullopt)
				: m_destination(std::move(destination)), m_increment_amount(std::move(increment_amount)) {}

			const destination_type& destination() const noexcept { return m_destination; }
			const std::optional<std::unique_ptr<expression>>& increment_amount() const noexcept { return m_increment_amount; }

			const typing::qual_type get_type() const override { return std::visit([](auto&& destination) {
				return destination->get_type();
			}, m_destination); }

			std::unique_ptr<expression> release_destination() noexcept { 
				if (std::holds_alternative<std::unique_ptr<expression>>(m_destination)) {
					return std::move(std::get<std::unique_ptr<expression>>(m_destination));
				}
				return nullptr;
			}

			void mutable_accept(visitor& v) override { v.visit(*this); }
			void accept(const_visitor& v) const override { v.visit(*this); }
		};

		class arithmetic_operator final : public expression {
		private:
			token_type m_operator;
			std::unique_ptr<expression> m_left;
			std::unique_ptr<expression> m_right;

            typing::qual_type m_result_type;
		public:
			arithmetic_operator(token_type op,
				std::unique_ptr<expression>&& left,
				std::unique_ptr<expression>&& right,
				typing::qual_type&& result_type)
				: m_operator(op),
				m_left(std::move(left)),
				m_right(std::move(right)),
				m_result_type(std::move(result_type)) {}

			token_type get_operator() const noexcept { return m_operator; }
			const std::unique_ptr<expression>& left() const noexcept { return m_left; }
			const std::unique_ptr<expression>& right() const noexcept { return m_right; }

			std::unique_ptr<expression> release_left() noexcept { return std::move(m_left); }
			std::unique_ptr<expression> release_right() noexcept { return std::move(m_right); }
			
            const typing::qual_type get_type() const override { return m_result_type; }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_left->mutable_accept(v);
				m_right->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
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
            const std::unique_ptr<expression>& operand() const noexcept { return m_operand; }

            const typing::qual_type get_type() const override { return m_operand->get_type(); }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_operand->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
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

			const std::unique_ptr<expression>& operand() const noexcept { return m_operand; }
			std::unique_ptr<expression> release_operand() noexcept { return std::move(m_operand); }
			const typing::qual_type get_type() const override { return m_target_type; }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_operand->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_operand->accept(v);
			}
		};

		class dereference final : public expression {
		private:
			std::unique_ptr<expression> m_operand;
		public:
			explicit dereference(std::unique_ptr<expression>&& operand) : m_operand(std::move(operand)) {}

			const std::unique_ptr<expression>& operand() const noexcept { return m_operand; }

			std::unique_ptr<expression> release_operand() noexcept { return std::move(m_operand); }

			const typing::qual_type get_type() const override {
                auto operand_type = m_operand->get_type();
				auto* ptr_type = dynamic_cast<const typing::pointer_type*>(operand_type.type().get());
                if (ptr_type == nullptr) {
                    throw std::runtime_error("Operand is not a pointer");
                }
				return ptr_type->pointee_type().to_owning();
			}

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_operand->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_operand->accept(v);
			}
		};

		class member_access final : public expression {
		private:
			std::unique_ptr<expression> m_base;
			const typing::member m_member;

			bool m_is_dereference;
		public:
			member_access(std::unique_ptr<expression>&& base, const typing::member member, bool is_dereference = false)
				: m_base(std::move(base)), m_member(member), m_is_dereference(is_dereference) {}

			const std::unique_ptr<expression>& base() const noexcept { return m_base; }
			const typing::member& member() const noexcept { return m_member; }
			bool is_dereference() const noexcept { return m_is_dereference; }
			const typing::qual_type get_type() const override { return m_member.member_type.to_owning(m_base->get_type().propagate_qualifiers()); }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_base->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
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

			const std::unique_ptr<expression>& base() const noexcept { return m_base; }
			const std::unique_ptr<expression>& index() const noexcept { return m_index; }

			std::unique_ptr<expression> release_index() noexcept { return std::move(m_index); }

			const typing::qual_type get_type() const override {
				auto base_type = m_base->get_type();

				std::shared_ptr<typing::array_type> arr_type = std::dynamic_pointer_cast<typing::array_type>(base_type.type());
				if (arr_type) {
					return arr_type->element_type().to_owning(base_type.propagate_qualifiers());
				}

				std::shared_ptr<typing::pointer_type> ptr_type = std::dynamic_pointer_cast<typing::pointer_type>(base_type.type());
				if (ptr_type) {
					return ptr_type->pointee_type().to_owning(base_type.propagate_qualifiers());
				}

				throw std::runtime_error("Base is not an array or pointer");
            }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_base->mutable_accept(v);
				m_index->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_base->accept(v);
				m_index->accept(v);
			}
		};

		class address_of final : public expression {
		public:
			using operand_type = std::variant<std::shared_ptr<variable>, std::unique_ptr<array_index>, std::unique_ptr<member_access>>;
		private:
			operand_type m_operand;
		public:
			address_of(std::shared_ptr<variable>&& variable)
				: m_operand(std::move(variable)) {}

			address_of(std::unique_ptr<array_index>&& array_index)
				: m_operand(std::move(array_index)) {}

			address_of(std::unique_ptr<member_access>&& member_access)
				: m_operand(std::move(member_access)) {}

			const operand_type& operand() const noexcept { return m_operand; }

			std::unique_ptr<expression> release_operand() noexcept { 
				return std::visit(overloaded{
					[&](std::shared_ptr<variable>& variable) -> std::unique_ptr<expression> {
						return std::make_unique<variable_reference>(std::move(variable));
					},
					[&](std::unique_ptr<array_index>& array_index) -> std::unique_ptr<expression> {
						return std::move(array_index);
					},
					[&](std::unique_ptr<member_access>& member_access) -> std::unique_ptr<expression> {
						return std::move(member_access);
					}
				}, m_operand);
			}

			const typing::qual_type get_type() const override { 
                return typing::qual_type(std::make_shared<typing::pointer_type>(std::visit(overloaded { 
                    [](const std::shared_ptr<variable>& variable) {
                        return variable->get_type();
                    },
                    [](const std::unique_ptr<array_index>& array_index) {
                        // Address of arr[i] is a pointer to the element type, not to the base type
                        return array_index->get_type();
                    },
                    [](const std::unique_ptr<member_access>& member_access) {
                        // Address of struct.member is a pointer to the member type
                        return member_access->get_type();
                    }
                }, m_operand)));
            }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				std::visit([&v](auto&& operand) {
					operand->mutable_accept(v);
				}, m_operand);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				std::visit([&v](auto&& operand) {
					operand->accept(v);
				}, m_operand);
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

			const std::unique_ptr<expression>& condition() const noexcept { return m_condition; }
			const std::unique_ptr<expression>& then_expression() const noexcept { return m_then_expression; }
			const std::unique_ptr<expression>& else_expression() const noexcept { return m_else_expression; }

			std::unique_ptr<expression> release_then_expression() noexcept { return std::move(m_then_expression); }
			std::unique_ptr<expression> release_else_expression() noexcept { return std::move(m_else_expression); }
			
            const typing::qual_type get_type() const override { 
                return m_result_type; 
            }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_condition->mutable_accept(v);
				m_then_expression->mutable_accept(v);
				m_else_expression->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_condition->accept(v);
				m_then_expression->accept(v);
				m_else_expression->accept(v);
			}
		};

		class set_address final : public expression {
		private:
			std::unique_ptr<expression> m_destination;
			std::unique_ptr<expression> m_value;

		public:
			set_address(std::unique_ptr<expression>&& destination, std::unique_ptr<expression>&& value)
				: m_destination(std::move(destination)), m_value(std::move(value)) {}

			const std::unique_ptr<expression>& destination() const noexcept { return m_destination; }
			const std::unique_ptr<expression>& value() const noexcept { return m_value; }

			std::unique_ptr<expression> release_value() noexcept { return std::move(m_value); }

			const typing::qual_type get_type() const override { return m_destination->get_type(); }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_destination->mutable_accept(v);
				m_value->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_destination->accept(v);
				m_value->accept(v);
			}
		};

		class set_variable final : public expression {
		private:
			std::shared_ptr<variable> m_variable;
			std::unique_ptr<expression> m_value;
		public:
			set_variable(std::shared_ptr<variable>&& variable, std::unique_ptr<expression>&& value)
				: m_variable(std::move(variable)), m_value(std::move(value)) {}

			const std::shared_ptr<variable>& variable() const noexcept { return m_variable; }
			const std::unique_ptr<expression>& value() const noexcept { return m_value; }

			std::unique_ptr<expression> release_value() noexcept { return std::move(m_value); }

			const typing::qual_type get_type() const override { return m_variable->get_type(); }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_variable->mutable_accept(v);
				m_value->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_variable->accept(v);
				m_value->accept(v);
			}
		};

		class statement : public mutable_visitable_base<visitor>, public const_visitable_base<const_visitor> {
		public:
			virtual ~statement() = default;
			virtual void mutable_accept(visitor& v) override = 0;
			virtual void accept(const_visitor& v) const override = 0;
		};

		class control_block : public symbol_context, public mutable_visitable_base<visitor>, public const_visitable_base<const_visitor>, public std::enable_shared_from_this<control_block> {
		private:
			std::vector<std::unique_ptr<statement>> m_statements;

		public:
			control_block()
				: symbol_context() {}

			bool is_implemented() const noexcept { return m_statements.size() > 0; }

			void implement(std::vector<std::unique_ptr<statement>>&& statements)
			{
				if (is_implemented()) {
					throw std::runtime_error("Control block is already implemented");
				}
				m_statements = std::move(statements);
			}

			const std::vector<std::unique_ptr<statement>>& statements() const noexcept { return m_statements; }

			std::vector<std::unique_ptr<statement>> release_statements() noexcept { return std::move(m_statements); }

			void mutable_accept(visitor& v) override {
				bool is_in_self = v.explorer().is_in_context(shared_from_this());

				if (!is_in_self) {
					v.explorer().visit(shared_from_this());
				}

				v.visit(*this);
				for (const auto& stmt : m_statements) {
					stmt->mutable_accept(v);
				}

				if (!is_in_self) {
					v.explorer().exit();
				}
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				for (const auto& stmt : m_statements) {
					stmt->accept(v);
				}
			}
			
			void transform(expression_transformer& expression_transformer, statement_transformer& statement_transformer);
		};

		class statement_block final : public statement {
		private:
			std::shared_ptr<control_block> m_control_block;

		public:
			statement_block(std::shared_ptr<control_block>&& control_block)
				: m_control_block(std::move(control_block)) {}

			const std::shared_ptr<control_block>& control_block() const noexcept { return m_control_block; }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_control_block->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_control_block->accept(v);
			}
		};

		class compound_expression final : public expression {
		private:
			std::shared_ptr<control_block> m_control_block;
			std::unique_ptr<expression> m_return_expression;

		public:
			compound_expression(std::shared_ptr<control_block>&& control_block, std::unique_ptr<expression>&& return_expression)
				: m_control_block(std::move(control_block)), m_return_expression(std::move(return_expression)) {}

			const std::shared_ptr<control_block>& control_block() const noexcept { return m_control_block; }
			const std::unique_ptr<expression>& return_expression() const noexcept { return m_return_expression; }

			std::unique_ptr<expression> release_return_expression() noexcept { return std::move(m_return_expression); }

			const typing::qual_type get_type() const override { return m_return_expression->get_type(); }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_control_block->mutable_accept(v);
				m_return_expression->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_control_block->accept(v);
				m_return_expression->accept(v);
			}
		};

		class expression_statement final : public statement {
		private:
			std::unique_ptr<expression> m_expression;
		public:
			explicit expression_statement(std::unique_ptr<expression>&& expr)
				: m_expression(std::move(expr)) {}

			const std::unique_ptr<expression>& expression() const noexcept { return m_expression; }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_expression->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_expression->accept(v);
			}
		};

		class variable_declaration final : public statement {
		private:
			std::shared_ptr<variable> m_variable;
			std::unique_ptr<expression> m_initializer;
		public:
			variable_declaration(std::shared_ptr<variable>&& var, std::unique_ptr<expression>&& initializer = nullptr)
				: m_variable(std::move(var)), m_initializer(std::move(initializer)) {}

			const std::shared_ptr<variable>& variable() const noexcept { return m_variable; }
			const std::unique_ptr<expression>& initializer() const noexcept { return m_initializer; }

			std::unique_ptr<expression> release_initializer() noexcept { return std::move(m_initializer); }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_variable->mutable_accept(v);
				if (m_initializer) {
					m_initializer->mutable_accept(v);
				}
			}

			void accept(const_visitor& v) const override {
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
			std::shared_ptr<function_definition> m_function;
		public:
			explicit return_statement(std::unique_ptr<expression>&& value = nullptr, std::shared_ptr<function_definition> function = nullptr)
				: m_value(std::move(value)), m_function(std::move(function)) {}

			const std::unique_ptr<expression>& value() const noexcept { return m_value; }
			const std::shared_ptr<function_definition>& function() const noexcept { return m_function; }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				if (m_value) {
					m_value->mutable_accept(v);
				}
			}

			void accept(const_visitor& v) const override {
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

			const std::unique_ptr<expression>& condition() const noexcept { return m_condition; }
			const std::shared_ptr<control_block>& then_body() const noexcept { return m_then_body; }
			const std::shared_ptr<control_block>& else_body() const noexcept { return m_else_body; }

			std::shared_ptr<control_block> release_then_body() noexcept { return std::move(m_then_body); }
			std::shared_ptr<control_block> release_else_body() noexcept { return std::move(m_else_body); }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_condition->mutable_accept(v);
				m_then_body->mutable_accept(v);
				if (m_else_body) {
					m_else_body->mutable_accept(v);
				}
			}

			void accept(const_visitor& v) const override {
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
			const std::unique_ptr<expression>& condition() const noexcept { return m_condition; }
			bool check_condition_first() const noexcept { return m_check_condition_first; }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				if (m_check_condition_first) {
					m_condition->mutable_accept(v);
				}
				v.explorer().visit(m_body);
				m_body->mutable_accept(v);
				if (!m_check_condition_first) {
					m_condition->mutable_accept(v);
				}
				v.explorer().exit();
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_body->accept(v);
				m_condition->accept(v);
			}
		};

		class break_statement final : public statement {
		private:
			int m_loop_depth;
		public:
			explicit break_statement(int loop_depth) : m_loop_depth(loop_depth) {}

			int loop_depth() const noexcept { return m_loop_depth; }

		public:
			void mutable_accept(visitor& v) override { v.visit(*this); }

			void accept(const_visitor& v) const override { v.visit(*this); }
		};

		class continue_statement final : public statement {
		private:
			int m_loop_depth;
		public:
			explicit continue_statement(int loop_depth) : m_loop_depth(loop_depth) {}

			int loop_depth() const noexcept { return m_loop_depth; }

		public:
			void mutable_accept(visitor& v) override { v.visit(*this); }

			void accept(const_visitor& v) const override { v.visit(*this); }
		};

		class function_definition final : public symbol, public control_block {
		private:
			typing::qual_type m_return_type;
			std::vector<std::shared_ptr<variable>> m_parameters;
			source_location m_location;

		public:
			explicit function_definition(std::string&& name, typing::qual_type&& return_type, std::vector<std::shared_ptr<variable>>&& parameters, std::weak_ptr<symbol_context>&& context, source_location&& location)
				: symbol(std::move(name), std::move(context)), control_block(), m_return_type(std::move(return_type)), m_parameters(std::move(parameters)), m_location(std::move(location)) {}

			const typing::qual_type& return_type() const noexcept { return m_return_type; }
			const std::vector<std::shared_ptr<variable>>& parameters() const noexcept { return m_parameters; }
			const source_location& location() const noexcept { return m_location; }

            std::string to_string() const noexcept override {
                return std::format("function {}", name());
            }

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				for (const auto& param : m_parameters) {
					param->mutable_accept(v);
				}
				
				control_block::mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				for (const auto& param : m_parameters) {
					param->accept(v);
				}
				
				control_block::accept(v);
			}
		};

		class function_reference final : public expression {
		private:
			std::shared_ptr<function_definition> m_function;
		public:
			explicit function_reference(std::shared_ptr<function_definition>&& func) : m_function(std::move(func)) {}

			const std::shared_ptr<function_definition>& get_function() const noexcept { return m_function; }

			const typing::qual_type get_type() const override {
				std::vector<typing::qual_type> param_types;
				for (const auto& param : m_function->parameters()) {
					param_types.push_back(param->get_type());
				}
				return typing::qual_type::owning(
					std::make_shared<typing::function_pointer_type>(
						m_function->return_type(),
						std::move(param_types)
					)
				);
			}

			void mutable_accept(visitor& v) override { v.visit(*this); }

			void accept(const_visitor& v) const override { v.visit(*this); }
		};

		class array_initializer final : public expression {
		private:
			std::vector<std::unique_ptr<expression>> m_initializers;
			typing::qual_type m_element_type;

		public:
			array_initializer(std::vector<std::unique_ptr<expression>>&& initializers, typing::qual_type&& element_type)
				: m_initializers(std::move(initializers)), m_element_type(std::move(element_type)) {}

			const std::vector<std::unique_ptr<expression>>& initializers() const noexcept { return m_initializers; }

			std::vector<std::unique_ptr<expression>> release_initializers() noexcept { return std::move(m_initializers); }
			
			const typing::qual_type& element_type() const noexcept { return m_element_type; }

			const typing::qual_type get_type() const override { 
				return typing::qual_type::owning(std::make_shared<typing::array_type>(m_element_type), m_element_type.propagate_qualifiers()); 
			}

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				for (const auto& initializer : m_initializers) {
					initializer->mutable_accept(v);
				}
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				for (const auto& initializer : m_initializers) {
					initializer->accept(v);
				}
			}
		};

		class allocate_array final : public expression {
		private:
			std::vector<std::unique_ptr<expression>> m_dimensions;
			typing::qual_type m_element_type;	
		public:
			allocate_array(std::vector<std::unique_ptr<expression>>&& dimensions, typing::qual_type&& element_type)
				: m_dimensions(std::move(dimensions)), m_element_type(std::move(element_type)) {}
				
			const std::vector<std::unique_ptr<expression>>& dimensions() const noexcept { return m_dimensions; }

			const typing::qual_type get_type() const override {
				return typing::qual_type::owning(std::make_shared<typing::array_type>(m_element_type));
			}

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				for (const auto& dimension : m_dimensions) {
					dimension->mutable_accept(v);
				}
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				for (const auto& dimension : m_dimensions) {
					dimension->accept(v);
				}
			}
		};
		
		class struct_initializer final : public expression {
		public:
			struct member_initializer {
				std::string member_name;
				std::unique_ptr<expression> initializer;
			};
		private:
			std::vector<member_initializer> m_initializers;
			std::shared_ptr<typing::struct_type> m_struct_type;
		public:
			struct_initializer(std::vector<member_initializer>&& initializers, std::shared_ptr<typing::struct_type>&& struct_type)
				: m_initializers(std::move(initializers)), m_struct_type(std::move(struct_type)) {}

			const std::vector<member_initializer>& initializers() const noexcept { return m_initializers; }

			std::vector<member_initializer> release_initializers() noexcept { return std::move(m_initializers); }

			const std::shared_ptr<typing::struct_type>& struct_type() const noexcept { return m_struct_type; }

			const typing::qual_type get_type() const override {
				return typing::qual_type::weak(m_struct_type);
			}

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				for (const auto& initializer : m_initializers) {
					initializer.initializer->mutable_accept(v);
				}
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				for (const auto& initializer : m_initializers) {
					initializer.initializer->accept(v);
				}
			}
		};

		class union_initializer final : public expression {
		private:
			std::unique_ptr<expression> m_initializer;
			std::shared_ptr<typing::union_type> m_union_type;
			typing::member m_target_member;
		public:
			union_initializer(std::unique_ptr<expression>&& initializer, std::shared_ptr<typing::union_type>&& union_type, typing::member&& target_member)
				: m_initializer(std::move(initializer)), m_union_type(std::move(union_type)), m_target_member(std::move(target_member)) {}

			const std::unique_ptr<expression>& initializer() const noexcept { return m_initializer; }
			const std::shared_ptr<typing::union_type>& union_type() const noexcept { return m_union_type; }

			std::unique_ptr<expression> release_initializer() noexcept { return std::move(m_initializer); }

			const typing::member& target_member() const noexcept { return m_target_member; }

			const typing::qual_type get_type() const override {
				return typing::qual_type::weak(m_union_type);
			}

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				m_initializer->mutable_accept(v);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				m_initializer->accept(v);
			}
		};

		class enumerator_literal final : public expression {
		private:
			typing::enum_type::enumerator m_enumerator;
			std::shared_ptr<typing::enum_type> m_enum_type;
		public:
			enumerator_literal(typing::enum_type::enumerator&& enumerator, std::shared_ptr<typing::enum_type> enum_type)
				: m_enumerator(std::move(enumerator)), m_enum_type(std::move(enum_type)) {}

			const typing::enum_type::enumerator& enumerator() const noexcept { return m_enumerator; }
			const std::shared_ptr<typing::enum_type>& enum_type() const noexcept { return m_enum_type; }

			const typing::qual_type get_type() const override {
				return typing::qual_type::weak(m_enum_type);
			}

			void mutable_accept(visitor& v) override {
				v.visit(*this);
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
			}
		};

		class enumerator_symbol final : public symbol {
		private:
			typing::enum_type::enumerator m_enumerator;
			std::shared_ptr<typing::enum_type> m_enum_type;
		public:
			enumerator_symbol(typing::enum_type::enumerator&& enumerator, std::shared_ptr<typing::enum_type>&& enum_type, std::weak_ptr<symbol_context>&& context)
				: symbol(std::move(enumerator.name), std::move(context)), m_enumerator(std::move(enumerator)), m_enum_type(std::move(enum_type)) {}

			const typing::enum_type::enumerator& enumerator() const noexcept { return m_enumerator; }
			const std::shared_ptr<typing::enum_type>& enum_type() const noexcept { return m_enum_type; }

			std::string to_string() const noexcept override {
				return std::format("enumerator {} of enum {}", m_enumerator.name, m_enum_type->name().value());
			}

			std::unique_ptr<expression> to_literal() const {
				return std::make_unique<enumerator_literal>(typing::enum_type::enumerator(m_enumerator.name, m_enumerator.value), std::shared_ptr<typing::enum_type>(m_enum_type));
			}
		};

		class function_call final : public expression {
		public:
			using callable = std::variant<std::shared_ptr<function_definition>, std::shared_ptr<expression>>;
		private:
			callable m_callee;
			std::vector<std::unique_ptr<expression>> m_arguments;
		public:
			function_call(callable&& callee,
				std::vector<std::unique_ptr<expression>>&& arguments)
				: m_callee(std::move(callee)), m_arguments(std::move(arguments)) {}

			const callable& callee() const noexcept { return m_callee; }
			const std::vector<std::unique_ptr<expression>>& arguments() const noexcept { return m_arguments; }

			const typing::qual_type get_type() const override {
				return std::visit(overloaded { 
					[](const std::shared_ptr<function_definition>& function) {
						return function->return_type();
					}, [](const std::shared_ptr<expression>& expression) {
						auto type = expression->get_type();
						if (!type.is_same_type<typing::function_pointer_type>()) {
							throw std::runtime_error("Callee is not a function pointer");
						}
						return std::dynamic_pointer_cast<typing::function_pointer_type>(type.type())->return_type();
					}
				}, m_callee);	
			}

			void mutable_accept(visitor& v) override {
				v.visit(*this);
				if (std::holds_alternative<std::shared_ptr<expression>>(m_callee)) {
					std::get<std::shared_ptr<expression>>(m_callee)->mutable_accept(v);
				} 
				for (const auto& arg : m_arguments) {
					arg->mutable_accept(v);
				}
			}

			void accept(const_visitor& v) const override {
				v.visit(*this);
				if (std::holds_alternative<std::shared_ptr<expression>>(m_callee)) {
					std::get<std::shared_ptr<expression>>(m_callee)->accept(v);
				}
				for (const auto& arg : m_arguments) {
					arg->accept(v);
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
			
			std::vector<variable_declaration> m_static_variable_declarations;
		public:
            translation_unit() : m_global_context(std::make_shared<symbol_context>()) {}

            void declare_struct(std::shared_ptr<typing::struct_type>&& struct_type) {
                std::string name = struct_type->name().value();
                m_structs[name] = std::move(struct_type);
            }

            void declare_union(std::shared_ptr<typing::union_type>&& union_type) {
                std::string name = union_type->name().value();
                m_unions[name] = std::move(union_type);
            }

            void declare_enum(std::shared_ptr<typing::enum_type>&& enum_type) {
                std::string name = enum_type->name().value();
                m_enums[name] = std::move(enum_type);
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

			void add_static_variable_declaration(variable_declaration&& declaration) {
				m_static_variable_declarations.push_back(std::move(declaration));
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
			const std::vector<variable_declaration>& static_variable_declarations() const noexcept { return m_static_variable_declarations; }

            const std::shared_ptr<symbol_context>& global_context() const noexcept { return m_global_context; }

			void mutable_accept(visitor& v) {
				v.visit(*this);
				for (const auto& sym : m_global_context->symbols()) {
					auto* var = dynamic_cast<variable*>(sym.get());
					if (var) {
						var->mutable_accept(v);
						continue;
					}
					auto* func = dynamic_cast<function_definition*>(sym.get());
					if (func) {
						func->mutable_accept(v);
					}
				}
			}

			void accept(const_visitor& v) const {
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

			void transform(expression_transformer& expression_transformer, statement_transformer& statement_transformer);
		};

		// Utility function to print the IR as a tree
		std::string to_tree_string(const translation_unit& unit);
	}
}

#endif
