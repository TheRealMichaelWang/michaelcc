#ifndef MICHAELCC_AST_HPP
#define MICHAELCC_AST_HPP

#include <memory>
#include <vector>
#include <optional>
#include <cstdint>
#include <string>

#include "tokens.hpp"
#include "errors.hpp"
#include "typing.hpp"
#include "utils.hpp"

namespace michaelcc {
	namespace ast {
        class ast_element;
        class type_specifier;
        class qualified_type;
        class derived_type;
        class function_type;
        class context_block;
        class for_loop;
        class do_block;
        class while_block;
        class if_block;
        class if_else_block;
        class return_statement;
        class break_statement;
        class continue_statement;
        class int_literal;
        class float_literal;
        class double_literal;
        class string_literal;
        class variable_reference;
        class get_index;
        class get_property;
        class set_operator;
        class dereference_operator;
        class get_reference;
        class arithmetic_operator;
        class conditional_expression;
        class function_call;
        class initializer_list_expression;
        class variable_declaration;
        class typedef_declaration;
        class struct_declaration;
        class enum_declaration;
        class union_declaration;
        class function_prototype;
        class function_declaration;

        using visitor = generic_visitor<
            type_specifier,
            qualified_type,
            derived_type,
            function_type,
            context_block,
            for_loop,
            do_block,
            while_block,
            if_block,
            if_else_block,
            return_statement,
            break_statement,
            continue_statement,
            int_literal,
            float_literal,
            double_literal,
            string_literal,
            variable_reference,
            get_index,
            get_property,
            set_operator,
            dereference_operator,
            get_reference,
            arithmetic_operator,
            conditional_expression,
            function_call,
            initializer_list_expression,
            variable_declaration,
            typedef_declaration,
            struct_declaration,
            enum_declaration,
            union_declaration,
            function_prototype,
            function_declaration
        >;

        template<typename ReturnType>
        class type_dispatcher : public generic_dispatcher<ReturnType, ast_element, 
            type_specifier,
            qualified_type,
            derived_type,
            function_type,
            struct_declaration,
            union_declaration,
            enum_declaration
        > { };

        template<typename ReturnType>
        class const_type_dispatcher : public generic_dispatcher<ReturnType, ast_element, 
            const type_specifier,
            const qualified_type,
            const derived_type,
            const function_type,
            const struct_declaration,
            const union_declaration,
            const enum_declaration
        > { };

		class ast_element : public visitable_base<visitor> {
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

			virtual void accept(visitor& v) const = 0;

			virtual std::unique_ptr<ast_element> clone() const = 0;
		};

        class type_specifier final : public ast_element {
        private:
            std::vector<token_type> m_type_keywords;

        public:
            explicit type_specifier(std::vector<token_type>&& type_keywords, source_location&& location)
                : ast_element(std::move(location)), m_type_keywords(std::move(type_keywords)) {}

            const std::vector<token_type>& type_keywords() const noexcept { return m_type_keywords; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<type_specifier>(std::vector<token_type>(m_type_keywords), source_location(location()));
            }

            void accept(visitor& v) const override { v.visit(*this); }
        };

        class qualified_type final : public ast_element {
        private:
            uint8_t m_qualifiers;
            std::unique_ptr<ast_element> m_inner_type;

        public:
            qualified_type(uint8_t qualifiers, std::unique_ptr<ast_element>&& inner_type, source_location&& location)
                : ast_element(std::move(location)), m_qualifiers(qualifiers), m_inner_type(std::move(inner_type)) {}

            uint8_t qualifiers() const noexcept { return m_qualifiers; }
            const std::unique_ptr<ast_element>& inner_type() const noexcept { return m_inner_type; }
            bool is_const() const noexcept { return m_qualifiers & typing::CONST_TYPE_QUALIFIER; }
            bool is_volatile() const noexcept { return m_qualifiers & typing::VOLATILE_TYPE_QUALIFIER; }
            bool is_restrict() const noexcept { return m_qualifiers & typing::RESTRICT_TYPE_QUALIFIER; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<qualified_type>(m_qualifiers, m_inner_type->clone(), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_inner_type->accept(v);
            }
        };

        class derived_type final : public ast_element {
        public:
            enum class kind { POINTER, ARRAY };

        private:
            kind m_kind;
            std::unique_ptr<ast_element> m_inner_type;
            std::optional<std::unique_ptr<ast_element>> m_array_size;

        public:
            explicit derived_type(std::unique_ptr<ast_element>&& inner_type, source_location&& location)
                : ast_element(std::move(location)), m_kind(kind::POINTER), 
                  m_inner_type(std::move(inner_type)), m_array_size(std::nullopt) {}

            derived_type(std::unique_ptr<ast_element>&& inner_type, 
                        std::optional<std::unique_ptr<ast_element>>&& array_size,
                        source_location&& location)
                : ast_element(std::move(location)), m_kind(kind::ARRAY),
                  m_inner_type(std::move(inner_type)), m_array_size(std::move(array_size)) {}

            kind type_kind() const noexcept { return m_kind; }
            bool is_pointer() const noexcept { return m_kind == kind::POINTER; }
            bool is_array() const noexcept { return m_kind == kind::ARRAY; }
            const std::unique_ptr<ast_element>& inner_type() const noexcept { return m_inner_type; }
            const std::optional<std::unique_ptr<ast_element>>& array_size() const noexcept { return m_array_size; }

            std::unique_ptr<ast_element> clone() const override {
                if (m_kind == kind::POINTER) {
                    return std::make_unique<derived_type>(m_inner_type->clone(), source_location(location()));
                }
                return std::make_unique<derived_type>(
                    m_inner_type->clone(),
                    m_array_size.has_value() ? std::make_optional(m_array_size.value()->clone()) : std::nullopt,
                    source_location(location())
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_inner_type->accept(v);
                if (m_array_size.has_value()) {
                    m_array_size.value()->accept(v);
                }
            }
        };

        class function_type final : public ast_element {
        public:
            struct parameter {
                std::unique_ptr<ast_element> param_type;
                std::optional<std::string> param_name;

                parameter(std::unique_ptr<ast_element>&& type, std::optional<std::string>&& name = std::nullopt)
                    : param_type(std::move(type)), param_name(std::move(name)) {}
            };

        private:
            std::unique_ptr<ast_element> m_return_type;
            std::vector<parameter> m_parameters;
            bool m_is_variadic;

        public:
            function_type(std::unique_ptr<ast_element>&& return_type,
                         std::vector<parameter>&& parameters,
                         bool is_variadic,
                         source_location&& location)
                : ast_element(std::move(location)), m_return_type(std::move(return_type)),
                  m_parameters(std::move(parameters)), m_is_variadic(is_variadic) {}

            const ast_element* return_type() const noexcept { return m_return_type.get(); }
            const std::vector<parameter>& parameters() const noexcept { return m_parameters; }
            bool is_variadic() const noexcept { return m_is_variadic; }

            std::unique_ptr<ast_element> clone() const override {
                std::vector<parameter> cloned_params;
                cloned_params.reserve(m_parameters.size());
                for (const auto& p : m_parameters) {
                    cloned_params.emplace_back(
                        p.param_type->clone(),
                        p.param_name.has_value() ? std::make_optional(std::string(p.param_name.value())) : std::nullopt
                    );
                }
                return std::make_unique<function_type>(
                    m_return_type->clone(), std::move(cloned_params), m_is_variadic, source_location(location())
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_return_type->accept(v);
                for (const auto& param : m_parameters) {
                    param.param_type->accept(v);
                }
            }
        };

		class context_block : public ast_element {
		private:
			std::vector<std::unique_ptr<ast_element>> m_statements;

		public:
			explicit context_block(std::vector<std::unique_ptr<ast_element>>&& statements, source_location&& location)
				: ast_element(std::move(location)), m_statements(std::move(statements)) { }

			// Move constructor
			context_block(context_block&& other) noexcept
				: ast_element(source_location(other.location())), m_statements(std::move(other.m_statements)) { }

			// Move assignment
			context_block& operator=(context_block&& other) noexcept {
				if (this != &other) {
					m_statements = std::move(other.m_statements);
				}
				return *this;
			}

			// Delete copy operations
			context_block(const context_block&) = delete;
			context_block& operator=(const context_block&) = delete;

			const std::vector<std::unique_ptr<ast_element>>& statements() const noexcept { return m_statements; }

			std::unique_ptr<ast_element> clone() const override {
				std::vector<std::unique_ptr<ast_element>> cloned;
				cloned.reserve(m_statements.size());
				for (const auto& stmt : m_statements) {
					cloned.push_back(stmt->clone());
				}
				return std::make_unique<context_block>(std::move(cloned), source_location(location()));
			}

			void accept(visitor& v) const override {
				v.visit(*this);
				for (const auto& statement : m_statements) {
					statement->accept(v);
				}
			}
		};

		class for_loop final : public ast_element {
		private:
			std::unique_ptr<ast_element> m_initial_statement;
			std::unique_ptr<ast_element> m_condition;
			std::unique_ptr<ast_element> m_increment_statement;
			context_block m_to_execute;

		public:
			for_loop(std::unique_ptr<ast_element>&& initial_statement,
				std::unique_ptr<ast_element>&& condition,
				std::unique_ptr<ast_element>&& increment_statement,
				context_block&& to_execute, source_location&& location)
				: ast_element(std::move(location)),
				m_initial_statement(std::move(initial_statement)),
				m_condition(std::move(condition)),
				m_increment_statement(std::move(increment_statement)),
				m_to_execute(std::move(to_execute)) { }

			const ast_element* initial_statement() const noexcept { return m_initial_statement.get(); }
			const ast_element* condition() const noexcept { return m_condition.get(); }
			const ast_element* increment_statement() const noexcept { return m_increment_statement.get(); }
			const context_block& body() const noexcept { return m_to_execute; }

			std::unique_ptr<ast_element> clone() const override {
				auto cloned_body = m_to_execute.clone();
				return std::make_unique<for_loop>(
					m_initial_statement->clone(),
					m_condition->clone(),
					m_increment_statement->clone(),
					std::move(*static_cast<context_block*>(cloned_body.release())),
					source_location(location())
				);
			}

			void accept(visitor& v) const override {
				v.visit(*this);
				m_initial_statement->accept(v);
				m_condition->accept(v);
				m_increment_statement->accept(v);
				m_to_execute.accept(v);
			}
		};

        class do_block final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_condition;
            context_block m_to_execute;

        public:
            do_block(std::unique_ptr<ast_element>&& condition, context_block&& to_execute, source_location&& location)
                : ast_element(std::move(location)),
                m_condition(std::move(condition)),
                m_to_execute(std::move(to_execute)) {}

            const ast_element* condition() const noexcept { return m_condition.get(); }
            const context_block& body() const noexcept { return m_to_execute; }

            std::unique_ptr<ast_element> clone() const override {
                auto cloned_body = m_to_execute.clone();
                return std::make_unique<do_block>(
                    m_condition->clone(),
                    std::move(*static_cast<context_block*>(cloned_body.release())),
                    source_location(location())
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_condition->accept(v);
                m_to_execute.accept(v);
            }
        };

        class while_block final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_condition;
            context_block m_to_execute;

        public:
            while_block(std::unique_ptr<ast_element>&& condition, context_block&& to_execute, source_location&& location)
                : ast_element(std::move(location)),
                m_condition(std::move(condition)),
                m_to_execute(std::move(to_execute)) {}

            const ast_element* condition() const noexcept { return m_condition.get(); }
            const context_block& body() const noexcept { return m_to_execute; }

            std::unique_ptr<ast_element> clone() const override {
                auto cloned_body = m_to_execute.clone();
                return std::make_unique<while_block>(
                    m_condition->clone(),
                    std::move(*static_cast<context_block*>(cloned_body.release())),
                    source_location(location())
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_condition->accept(v);
                m_to_execute.accept(v);
            }
        };

        class if_block final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_condition;
            context_block m_execute_if_true;

        public:
            if_block(std::unique_ptr<ast_element>&& condition, context_block&& execute_if_true, source_location&& location)
                : ast_element(std::move(location)),
                m_condition(std::move(condition)),
                m_execute_if_true(std::move(execute_if_true)) {}

            const ast_element* condition() const noexcept { return m_condition.get(); }
            const context_block& body() const noexcept { return m_execute_if_true; }

            std::unique_ptr<ast_element> clone() const override {
                auto cloned_body = m_execute_if_true.clone();
                return std::make_unique<if_block>(
                    m_condition->clone(),
                    std::move(*static_cast<context_block*>(cloned_body.release())),
                    source_location(location())
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_condition->accept(v);
                m_execute_if_true.accept(v);
            }
        };

        class if_else_block final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_condition;
            context_block m_execute_if_true;
            context_block m_execute_if_false;

        public:
            if_else_block(std::unique_ptr<ast_element>&& condition,
                context_block&& execute_if_true,
                context_block&& execute_if_false,
                source_location&& location)
                : ast_element(std::move(location)),
                m_condition(std::move(condition)),
                m_execute_if_true(std::move(execute_if_true)),
                m_execute_if_false(std::move(execute_if_false)) {}

            const ast_element* condition() const noexcept { return m_condition.get(); }
            const context_block& true_body() const noexcept { return m_execute_if_true; }
            const context_block& false_body() const noexcept { return m_execute_if_false; }

            std::unique_ptr<ast_element> clone() const override {
                auto cloned_true = m_execute_if_true.clone();
                auto cloned_false = m_execute_if_false.clone();
                return std::make_unique<if_else_block>(
                    m_condition->clone(),
                    std::move(*static_cast<context_block*>(cloned_true.release())),
                    std::move(*static_cast<context_block*>(cloned_false.release())),
                    source_location(location())
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_condition->accept(v);
                m_execute_if_true.accept(v);
                m_execute_if_false.accept(v);
            }
        };

        class return_statement final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_value;

        public:
            explicit return_statement(std::unique_ptr<ast_element>&& return_value, source_location&& location)
                : ast_element(std::move(location)),
                m_value(std::move(return_value)) {}

            const ast_element* value() const noexcept { return m_value.get(); }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<return_statement>(
                    m_value ? m_value->clone() : nullptr,
                    source_location(location())
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                if (m_value) {
                    m_value->accept(v);
                }
            }
        };

        class break_statement final : public ast_element {
        private:
            int m_loop_depth;

        public:
            explicit break_statement(int depth, source_location&& location)
                : ast_element(std::move(location)),
                m_loop_depth(depth) {}

            int depth() const noexcept { return m_loop_depth; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<break_statement>(m_loop_depth, source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
            }
        };

        class continue_statement final : public ast_element {
        private:
            int m_loop_depth;

        public:
            explicit continue_statement(int depth, source_location&& location)
                : ast_element(std::move(location)), m_loop_depth(depth) {}

            int depth() const noexcept { return m_loop_depth; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<continue_statement>(m_loop_depth, source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
            }
        };

        class int_literal final : public ast_element {
        private:
            int64_t m_value;

        public:
            explicit int_literal(int64_t value, source_location&& location)
                : ast_element(std::move(location)), m_value(value) {}

            int64_t value() const noexcept { return m_value; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<int_literal>(m_value, source_location(location()));
            }

            void accept(visitor& v) const override { v.visit(*this); }
        };

        class float_literal final : public ast_element {
        private:
            float m_value;

        public:
            explicit float_literal(float value, source_location&& location)
                : ast_element(std::move(location)),
                m_value(value) {}

            float value() const noexcept { return m_value; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<float_literal>(m_value, source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
            }
        };

        class double_literal final : public ast_element {
        private:
            double m_value;

        public:
            explicit double_literal(double value, source_location&& location)
                : ast_element(std::move(location)),
                m_value(value) {}

            double value() const noexcept { return m_value; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<double_literal>(m_value, source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
            }
        };

        class string_literal final : public ast_element {
        private:
            std::string m_value;
        public:
            explicit string_literal(std::string value, source_location&& location)
                : ast_element(std::move(location)), m_value(std::move(value)) {}

            const std::string& value() const noexcept { return m_value; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<string_literal>(m_value, source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
            }
        };

        class variable_reference final : public ast_element {
        private:
            std::string m_identifier;

        public:
            explicit variable_reference(const std::string& identifier, source_location&& location)
                : ast_element(std::move(location)),
                m_identifier(identifier) {}

            const std::string& identifier() const noexcept { return m_identifier; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<variable_reference>(m_identifier, source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
            }
        };

        class get_index final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_ptr;
            std::unique_ptr<ast_element> m_index;

        public:
            get_index(std::unique_ptr<ast_element>&& ptr, std::unique_ptr<ast_element>&& index, source_location&& location)
                : ast_element(std::move(location)),
                m_ptr(std::move(ptr)),
                m_index(std::move(index)) {}

            const ast_element* pointer() const noexcept { return m_ptr.get(); }
            const ast_element* index() const noexcept { return m_index.get(); }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<get_index>(m_ptr->clone(), m_index->clone(), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_ptr->accept(v);
                m_index->accept(v);
            }
        };

        class get_property final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_struct;
            std::string m_property_name;
            bool m_is_pointer_dereference;

        public:
            get_property(std::unique_ptr<ast_element>&& m_struct, std::string&& property_name, bool is_pointer_dereference, source_location&& location)
                : ast_element(std::move(location)),
                m_struct(std::move(m_struct)),
                m_property_name(std::move(property_name)),
                m_is_pointer_dereference(is_pointer_dereference) {}

            const ast_element* struct_expr() const noexcept { return m_struct.get(); }
            const std::string& property_name() const noexcept { return m_property_name; }
            bool is_pointer_dereference() const noexcept { return m_is_pointer_dereference; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<get_property>(m_struct->clone(), std::string(m_property_name), m_is_pointer_dereference, source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_struct->accept(v);
            }
        };

        class set_operator final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_set_dest;
            std::unique_ptr<ast_element> m_set_value;

        public:
            set_operator(std::unique_ptr<ast_element>&& set_dest, std::unique_ptr<ast_element>&& set_value, source_location&& location)
                : ast_element(std::move(location)),
                m_set_dest(std::move(set_dest)),
                m_set_value(std::move(set_value)) {}

            const ast_element* destination() const noexcept { return m_set_dest.get(); }
            const ast_element* value() const noexcept { return m_set_value.get(); }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<set_operator>(m_set_dest->clone(), m_set_value->clone(), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_set_dest->accept(v);
                m_set_value->accept(v);
            }
        };

        class dereference_operator final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_pointer;

        public:
            explicit dereference_operator(std::unique_ptr<ast_element>&& pointer, source_location&& location)
                : ast_element(std::move(location)),
                m_pointer(std::move(pointer)) {}

            const ast_element* pointer() const noexcept { return m_pointer.get(); }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<dereference_operator>(m_pointer->clone(), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_pointer->accept(v);
            }
        };

        class get_reference final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_item;

        public:
            explicit get_reference(std::unique_ptr<ast_element>&& item, source_location&& location)
                : ast_element(std::move(location)),
                m_item(std::move(item)) {}

            const ast_element* item() const noexcept { return m_item.get(); }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<get_reference>(m_item->clone(), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_item->accept(v);
            }
        };

        class arithmetic_operator final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_right;
            std::unique_ptr<ast_element> m_left;
            token_type m_operation;

        public:
            const static std::map<token_type, int> operator_precedence;

            arithmetic_operator(token_type operation, std::unique_ptr<ast_element>&& left, std::unique_ptr<ast_element>&& right, source_location&& location)
                : ast_element(std::move(location)),
                m_right(std::move(right)),
                m_left(std::move(left)),
                m_operation(operation) {}

            token_type operation() const noexcept { return m_operation; }
            const ast_element* left() const noexcept { return m_left.get(); }
            const ast_element* right() const noexcept { return m_right.get(); }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<arithmetic_operator>(m_operation, m_left->clone(), m_right->clone(), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_left->accept(v);
                m_right->accept(v);
            }
        };

        class conditional_expression final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_condition;
            std::unique_ptr<ast_element> m_true_expr;
            std::unique_ptr<ast_element> m_false_expr;

        public:
            conditional_expression(std::unique_ptr<ast_element>&& condition,
                std::unique_ptr<ast_element>&& true_expr,
                std::unique_ptr<ast_element>&& false_expr,
                source_location&& location)
                : ast_element(std::move(location)),
                m_condition(std::move(condition)),
                m_true_expr(std::move(true_expr)),
                m_false_expr(std::move(false_expr)) {}

            const ast_element* condition() const noexcept { return m_condition.get(); }
            const ast_element* true_expr() const noexcept { return m_true_expr.get(); }
            const ast_element* false_expr() const noexcept { return m_false_expr.get(); }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<conditional_expression>(m_condition->clone(), m_true_expr->clone(), m_false_expr->clone(), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_condition->accept(v);
                m_true_expr->accept(v);
                m_false_expr->accept(v);
            }
        };

        class function_call : public ast_element {
        private:
            std::unique_ptr<ast_element> m_callee;
            std::vector<std::unique_ptr<ast_element>> m_arguments;

        public:
            function_call(std::unique_ptr<ast_element> callee,
                std::vector<std::unique_ptr<ast_element>> arguments,
                source_location location)
                : ast_element(std::move(location)),
                m_callee(std::move(callee)),
                m_arguments(std::move(arguments)) {}

            const ast_element* callee() const noexcept { return m_callee.get(); }
            const std::vector<std::unique_ptr<ast_element>>& arguments() const noexcept { return m_arguments; }

            std::unique_ptr<ast_element> clone() const override {
                std::vector<std::unique_ptr<ast_element>> cloned_arguments;
                cloned_arguments.reserve(m_arguments.size());
                for (const auto& arg : m_arguments) {
                    cloned_arguments.push_back(arg->clone());
                }
                return std::make_unique<function_call>(m_callee->clone(), std::move(cloned_arguments), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_callee->accept(v);
                for (const auto& arg : m_arguments) {
                    arg->accept(v);
                }
            }
        };

        class initializer_list_expression : public ast_element {
        private:
            std::vector<std::unique_ptr<ast_element>> m_initializers;
        public:
            explicit initializer_list_expression(std::vector<std::unique_ptr<ast_element>>&& initializers, source_location&& location)
                : ast_element(std::move(location)), m_initializers(std::move(initializers)) {}

            const std::vector<std::unique_ptr<ast_element>>& initializers() const noexcept { return m_initializers; }

            std::unique_ptr<ast_element> clone() const override {
                std::vector<std::unique_ptr<ast_element>> cloned_initializers;
                cloned_initializers.reserve(m_initializers.size());
                for (const auto& init : m_initializers) {
                    cloned_initializers.push_back(init->clone());
                }
                return std::make_unique<initializer_list_expression>(std::move(cloned_initializers), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                for (const auto& init : m_initializers) {
                    init->accept(v);
                }
            }
        };

        class variable_declaration final : public ast_element {
        private:
            uint8_t m_qualifiers;
            std::unique_ptr<ast_element> m_type;
            std::string m_identifier;
            std::optional<std::unique_ptr<ast_element>> m_set_value;

        public:
            variable_declaration(uint8_t qualifiers,
                std::unique_ptr<ast_element>&& var_type,
                const std::string& identifier, source_location&& location,
                std::optional<std::unique_ptr<ast_element>>&& set_value = std::nullopt)
                : ast_element(std::move(location)),
                m_qualifiers(qualifiers),
                m_type(std::move(var_type)),
                m_identifier(identifier),
                m_set_value(set_value ? std::make_optional<std::unique_ptr<ast_element>>(std::move(set_value.value())) : std::nullopt) {}

            uint8_t qualifiers() const noexcept { return m_qualifiers; }
            const ast_element* type() const noexcept { return m_type.get(); }
            const std::string& identifier() const noexcept { return m_identifier; }
            const ast_element* set_value() const noexcept { return m_set_value.has_value() ? m_set_value.value().get() : nullptr; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<variable_declaration>(
                    m_qualifiers,
                    m_type->clone(),
                    m_identifier,
                    source_location(location()),
                    m_set_value.has_value() ? std::make_optional(m_set_value.value()->clone()) : std::nullopt
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_type->accept(v);
                if (m_set_value.has_value()) {
                    m_set_value.value()->accept(v);
                }
            }
        };

        class typedef_declaration final : public ast_element {
        private:
            std::unique_ptr<ast_element> m_type;
            std::string m_name;

        public:
            typedef_declaration(std::unique_ptr<ast_element>&& type, std::string&& name, source_location&& location)
                : ast_element(std::move(location)),
                m_type(std::move(type)), m_name(std::move(name)) {}

            const ast_element* type() const noexcept { return m_type.get(); }
            const std::string& name() const noexcept { return m_name; }

            std::unique_ptr<ast_element> clone() const override {
                return std::make_unique<typedef_declaration>(m_type->clone(), std::string(m_name), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_type->accept(v);
            }
        };

        class struct_declaration final : public ast_element {
        private:
            std::optional<std::string> m_struct_name;
            std::vector<variable_declaration> m_fields;

        public:
            struct_declaration(std::optional<std::string>&& struct_name, std::vector<variable_declaration>&& feilds, source_location&& location)
                : ast_element(std::move(location)),
                m_struct_name(std::move(struct_name)), m_fields(std::move(feilds)) {}

            const std::optional<std::string>& struct_name() const noexcept { return m_struct_name; }
            const std::vector<variable_declaration>& feilds() const noexcept { return m_fields; }

            std::unique_ptr<ast_element> clone() const override {
                if (m_struct_name.has_value()) {
                    return std::make_unique<struct_declaration>(std::optional<std::string>(m_struct_name), std::vector<variable_declaration>(), source_location(location()));
                }

                std::vector<variable_declaration> cloned_feilds;
                cloned_feilds.reserve(m_fields.size());
                for (const variable_declaration& feild : m_fields) {
                    cloned_feilds.emplace_back(variable_declaration(
                        feild.qualifiers(),
                        feild.type()->clone(),
                        std::string(feild.identifier()),
                        source_location(feild.location()),
                        feild.set_value() != nullptr ? std::make_optional(feild.set_value()->clone()) : std::nullopt
                    ));
                }
                return std::make_unique<struct_declaration>(std::optional<std::string>(m_struct_name), std::move(cloned_feilds), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                for (const auto& feild : m_fields) {
                    feild.accept(v);
                }
            }
        };

        class enum_declaration final : public ast_element {
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

            const std::optional<std::string>& enum_name() const noexcept { return m_enum_name; }
            const std::vector<enumerator>& enumerators() const noexcept { return m_enumerators; }

            std::unique_ptr<ast_element> clone() const override {
                if (m_enum_name.has_value()) {
                    return std::make_unique<enum_declaration>(std::optional<std::string>(m_enum_name), std::vector<enumerator>(), source_location(location()));
                }

                std::vector<enumerator> cloned_enumerators;
                cloned_enumerators.reserve(m_enumerators.size());
                for (const auto& e : m_enumerators) {
                    cloned_enumerators.emplace_back(std::string(e.name), e.value);
                }
                return std::make_unique<enum_declaration>(std::optional<std::string>(m_enum_name), std::move(cloned_enumerators), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
            }
        };

        class union_declaration : public ast_element {
        public:
            struct member {
                std::unique_ptr<ast_element> member_type;
                std::string member_name;

                member(std::unique_ptr<ast_element>&& member_type, std::string&& member_name)
                    : member_type(std::move(member_type)), member_name(std::move(member_name)) {}
            };

        private:
            std::optional<std::string> m_union_name;
            std::vector<member> m_members;

        public:
            union_declaration(std::optional<std::string>&& union_name, std::vector<member>&& members, source_location&& location)
                : ast_element(std::move(location)),
                m_union_name(std::move(union_name)), m_members(std::move(members)) {}

            const std::optional<std::string>& union_name() const noexcept { return m_union_name; }
            const std::vector<member>& members() const noexcept { return m_members; }

            std::unique_ptr<ast_element> clone() const override {
                if (m_union_name.has_value()) {
                    return std::make_unique<union_declaration>(std::optional<std::string>(m_union_name), std::vector<member>(), source_location(location()));
                }

                std::vector<member> cloned_members;
                cloned_members.reserve(m_members.size());
                for (const auto& m : m_members) {
                    cloned_members.emplace_back(m.member_type->clone(), std::string(m.member_name));
                }
                return std::make_unique<union_declaration>(std::optional<std::string>(m_union_name), std::move(cloned_members), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                for (const auto& member : m_members) {
                    member.member_type->accept(v);
                }
            }
        };

        struct function_parameter {
            uint8_t qualifiers;
            std::unique_ptr<ast_element> param_type;
            std::string param_name;

            function_parameter(std::unique_ptr<ast_element>&& param_type, std::string&& param_name, uint8_t qualifiers)
                : param_type(std::move(param_type)), param_name(std::move(param_name)), qualifiers(qualifiers) {}
        };

        class function_prototype : public ast_element {
        private:
            std::unique_ptr<ast_element> m_return_type;
            std::string m_function_name;
            std::vector<function_parameter> m_parameters;

        public:
            function_prototype(std::unique_ptr<ast_element>&& return_type,
                std::string&& function_name,
                std::vector<function_parameter>&& parameters,
                source_location&& location)
                : ast_element(std::move(location)),
                m_return_type(std::move(return_type)),
                m_function_name(std::move(function_name)),
                m_parameters(std::move(parameters)) {}

            const ast_element* return_type() const noexcept { return m_return_type.get(); }
            const std::string& function_name() const noexcept { return m_function_name; }
            const std::vector<function_parameter>& parameters() const noexcept { return m_parameters; }

            std::unique_ptr<ast_element> clone() const override {
                std::vector<function_parameter> cloned_params;
                cloned_params.reserve(m_parameters.size());
                for (const auto& p : m_parameters) {
                    cloned_params.emplace_back(p.param_type->clone(), std::string(p.param_name), p.qualifiers);
                }
                return std::make_unique<function_prototype>(m_return_type->clone(), std::string(m_function_name), std::move(cloned_params), source_location(location()));
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_return_type->accept(v);
                for (const auto& param : m_parameters) {
                    param.param_type->accept(v);
                }
            }
        };

        class function_declaration : public ast_element {
        private:
            std::unique_ptr<ast_element> m_return_type;
            std::string m_function_name;
            std::vector<function_parameter> m_parameters;
            context_block m_function_body;

        public:
            function_declaration(std::unique_ptr<ast_element>&& return_type,
                std::string&& function_name,
                std::vector<function_parameter>&& parameters,
                context_block&& function_body,
                source_location&& location)
                : ast_element(std::move(location)),
                m_return_type(std::move(return_type)),
                m_function_name(std::move(function_name)),
                m_parameters(std::move(parameters)),
                m_function_body(std::move(function_body)) {}

            const ast_element* return_type() const noexcept { return m_return_type.get(); }
            const std::string& function_name() const noexcept { return m_function_name; }
            const std::vector<function_parameter>& parameters() const noexcept { return m_parameters; }
            const context_block& function_body() const noexcept { return m_function_body; }

            std::unique_ptr<ast_element> clone() const override {
                std::vector<function_parameter> cloned_params;
                cloned_params.reserve(m_parameters.size());
                for (const auto& p : m_parameters) {
                    cloned_params.emplace_back(p.param_type->clone(), std::string(p.param_name), p.qualifiers);
                }
                auto cloned_body = m_function_body.clone();
                return std::make_unique<function_declaration>(
                    m_return_type->clone(),
                    std::string(m_function_name),
                    std::move(cloned_params),
                    std::move(*static_cast<context_block*>(cloned_body.release())),
                    source_location(location())
                );
            }

            void accept(visitor& v) const override {
                v.visit(*this);
                m_return_type->accept(v);
                for (const auto& param : m_parameters) {
                    param.param_type->accept(v);
                }
                m_function_body.accept(v);
            }
        };

        // Utility function to convert any AST element to a C string representation
        std::string to_c_string(const ast_element* elem, int indent = 0);
	}
}

#endif
