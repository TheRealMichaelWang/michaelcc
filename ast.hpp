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

        class void_type;
        class int_type;
        class float_type;
        class pointer_type;
        class array_type;
        class function_pointer_type;
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

        class visitor {
        public:
            virtual ~visitor() = default;

            // Types
            virtual void visit(const int_type& node) { }
            virtual void visit(const float_type& node) { }
            virtual void visit(const pointer_type& node) { }
            virtual void visit(const array_type& node) { }
            virtual void visit(const function_pointer_type& node) { }

            // Statements
            virtual void visit(const context_block& node) { }
            virtual void visit(const for_loop& node) { }
            virtual void visit(const do_block& node) { }
            virtual void visit(const while_block& node) { }
            virtual void visit(const if_block& node) { }
            virtual void visit(const if_else_block& node) { }
            virtual void visit(const return_statement& node) { }
            virtual void visit(const break_statement& node) { }
            virtual void visit(const continue_statement& node) { }

            // Expressions
            virtual void visit(const int_literal& node) { }
            virtual void visit(const float_literal& node) { }
            virtual void visit(const double_literal& node) { }
            virtual void visit(const string_literal& node) { }
            virtual void visit(const variable_reference& node) { }
            virtual void visit(const get_index& node) { }
            virtual void visit(const get_property& node) { }
            virtual void visit(const set_operator& node) { }
            virtual void visit(const dereference_operator& node) { }
            virtual void visit(const get_reference& node) { }
            virtual void visit(const arithmetic_operator& node) { }
            virtual void visit(const conditional_expression& node) { }
            virtual void visit(const function_call& node) { }
            virtual void visit(const initializer_list_expression& node) { }

            // Top-level elements
            virtual void visit(const variable_declaration& node) { }
            virtual void visit(const typedef_declaration& node) { }
            virtual void visit(const struct_declaration& node) { }
            virtual void visit(const enum_declaration& node) { }
            virtual void visit(const union_declaration& node) { }
            virtual void visit(const function_prototype& node) { }
            virtual void visit(const function_declaration& node) { }
        };

		class ast_element {
		private:
			source_location m_location;

		public:
			explicit ast_element(source_location&& location) : m_location(std::move(location)) {

			}

			ast_element() : m_location() { }

			source_location location() const noexcept {
				return m_location;
			}

			virtual ~ast_element() = default;

			virtual void visit(std::unique_ptr<visitor>& visitor) const = 0;

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
			void visit(std::unique_ptr<visitor>& visitor) const override = 0;
		};

        class statement : virtual public ast_element {
        public:
            virtual void build_c_string_indent(std::stringstream& ss, int parent_precedence) const = 0;

            void build_c_string(std::stringstream& ss) const override {
                build_c_string_indent(ss, 0);
            }
            
            virtual std::unique_ptr<statement> clone_statement() const = 0;
            
            void visit(std::unique_ptr<visitor>& visitor) const override = 0;
        };

		class expression : virtual public ast_element {
		public:
			virtual void build_c_string_prec(std::stringstream& ss, int indent) const = 0;

			void build_c_string(std::stringstream& ss) const override {
				build_c_string_prec(ss, 0);
			}

			virtual std::unique_ptr<expression> clone_expression() const = 0;

			void visit(std::unique_ptr<visitor>& visitor) const override = 0;
		};

		class set_destination : public expression {
		public:
			void build_c_string_prec(std::stringstream& ss, int indent) const override = 0;

            virtual std::unique_ptr<set_destination> clone_set_destination() const = 0;

            std::unique_ptr<expression> clone_expression() const override {
                return clone_set_destination();
            }
		};

		class type : virtual public ast_element {
		public:
			virtual void build_declarator(std::stringstream& ss, const std::string& identifier) const {
				build_c_string(ss);
				ss << ' ' << identifier;
			}

			virtual std::unique_ptr<type> clone_type() const = 0;

			void visit(std::unique_ptr<visitor>& visitor) const override = 0;
		};

		class void_type : public type {
		public:
			void_type(source_location&& location)
				: ast_element(std::move(location)) { }

			void build_c_string(std::stringstream& ss) const override {
				ss << "void";
			}

			std::unique_ptr<type> clone_type() const override {
				return std::make_unique<void_type>(source_location(location()));
			}

			void visit(std::unique_ptr<visitor>& visitor) const override { }
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

			std::unique_ptr<type> clone_type() const override {
				return std::make_unique<int_type>(m_qualifiers, m_class, source_location(location()));
			}

			void visit(std::unique_ptr<visitor>& visitor) const override {
				visitor->visit(*this);
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

			std::unique_ptr<type> clone_type() const override {
				return std::make_unique<float_type>(m_class, source_location(location()));
			}

			void visit(std::unique_ptr<visitor>& visitor) const override {
				visitor->visit(*this);
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

			std::unique_ptr<type> clone_type() const override {
				return std::make_unique<pointer_type>(m_pointee_type->clone_type(), source_location(location()));
			}

			void visit(std::unique_ptr<visitor>& visitor) const override {
				visitor->visit(*this);
				m_pointee_type->visit(visitor);
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

			std::unique_ptr<type> clone_type() const override {
				return std::make_unique<array_type>(m_element_type->clone_type(), m_length ? std::make_optional(m_length.value()->clone_expression()) : std::nullopt, source_location(location()));
			}

			void visit(std::unique_ptr<visitor>& visitor) const override {
				visitor->visit(*this);
				m_element_type->visit(visitor);
				if (m_length.has_value()) {
					m_length.value()->visit(visitor);
				}
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

			std::unique_ptr<type> clone_type() const override {
				std::vector<std::unique_ptr<ast::type>> parameter_types;
				parameter_types.reserve(m_parameter_types.size());
				for (const auto& parameter : m_parameter_types) {
					parameter_types.emplace_back(parameter->clone_type());
				}
				return std::make_unique<function_pointer_type>(m_return_type->clone_type(), std::move(parameter_types), source_location(location()));
			}

			void visit(std::unique_ptr<visitor>& visitor) const override {
				visitor->visit(*this);
				m_return_type->visit(visitor);
				for (const auto& paramater : m_parameter_types) {
					paramater->visit(visitor);
				}
			}
		};

        class context_block : public statement {
        private:
            std::vector<std::unique_ptr<statement>> m_statements;

        public:
            explicit context_block(std::vector<std::unique_ptr<statement>>&& statements, source_location&& location)
                : ast_element(std::move(location)), m_statements(std::move(statements)) { }

            void build_c_string_indent(std::stringstream&, int) const override;

            context_block clone_block() const {
                std::vector<std::unique_ptr<statement>> cloned_statements;
                cloned_statements.reserve(m_statements.size());
                for (const auto& stmt : m_statements) {
                    cloned_statements.emplace_back(stmt->clone_statement());
                }
                return context_block(std::move(cloned_statements), source_location(location()));
            }

            std::unique_ptr<statement> clone_statement() const override {
                return std::make_unique<context_block>(clone_block());
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                for (const auto& statement : m_statements) {
                    statement->visit(visitor);
                }
            }
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

            std::unique_ptr<statement> clone_statement() const override {
                auto cloned_to_execute = m_to_execute.clone_block();
                return std::make_unique<for_loop>(
                    m_initial_statement->clone_statement(),
                    m_condition->clone_expression(),
                    m_increment_statement->clone_statement(),
                    std::move(cloned_to_execute),
                    source_location(location())
                );
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_initial_statement->visit(visitor);
                m_condition->visit(visitor);
                m_increment_statement->visit(visitor);
                m_to_execute.visit(visitor);
            }
        };

        class do_block final : public statement {
        private:
            std::unique_ptr<expression> m_condition;
            context_block m_to_execute;

        public:
            do_block(std::unique_ptr<expression>&& condition, context_block&& to_execute, source_location&& location)
                : ast_element(std::move(location)),
                m_condition(std::move(condition)),
                m_to_execute(std::move(to_execute)) {}

            void build_c_string_indent(std::stringstream&, int) const override;

            std::unique_ptr<statement> clone_statement() const override {
                auto cloned_to_execute = m_to_execute.clone_block();
                return std::make_unique<do_block>(
                    m_condition->clone_expression(),
                    std::move(cloned_to_execute),
                    source_location(location())
                );
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_condition->visit(visitor);
                m_to_execute.visit(visitor);
            }
        };

        class while_block final : public statement {
        private:
            std::unique_ptr<expression> m_condition;
            context_block m_to_execute;

        public:
            while_block(std::unique_ptr<expression>&& condition, context_block&& to_execute, source_location&& location)
                : ast_element(std::move(location)),
                m_condition(std::move(condition)),
                m_to_execute(std::move(to_execute)) {}

            void build_c_string_indent(std::stringstream&, int) const override;

            std::unique_ptr<statement> clone_statement() const override {
                auto cloned_to_execute = m_to_execute.clone_block();
                return std::make_unique<while_block>(
                    m_condition->clone_expression(),
                    std::move(cloned_to_execute),
                    source_location(location())
                );
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_condition->visit(visitor);
                m_to_execute.visit(visitor);
            }
        };

        class if_block final : public statement {
        private:
            std::unique_ptr<expression> m_condition;
            context_block m_execute_if_true;

        public:
            if_block(std::unique_ptr<expression>&& condition, context_block&& execute_if_true, source_location&& location)
                : ast_element(std::move(location)),
                m_condition(std::move(condition)),
                m_execute_if_true(std::move(execute_if_true)) {}

            void build_c_string_indent(std::stringstream&, int) const override;

            std::unique_ptr<statement> clone_statement() const override {
                auto cloned_execute_if_true = m_execute_if_true.clone_block();
                return std::make_unique<if_block>(
                    m_condition->clone_expression(),
                    std::move(cloned_execute_if_true),
                    source_location(location())
                );
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_condition->visit(visitor);
                m_execute_if_true.visit(visitor);
            }
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
                m_execute_if_false(std::move(execute_if_false)) {}

            void build_c_string_indent(std::stringstream&, int) const override;

            std::unique_ptr<statement> clone_statement() const override {
                auto cloned_execute_if_true = m_execute_if_true.clone_block();
                auto cloned_execute_if_false = m_execute_if_false.clone_block();
                return std::make_unique<if_else_block>(
                    m_condition->clone_expression(),
                    std::move(cloned_execute_if_true),
                    std::move(cloned_execute_if_false),
                    source_location(location())
                );
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_condition->visit(visitor);
                m_execute_if_true.visit(visitor);
                m_execute_if_false.visit(visitor);
            }
        };

        class return_statement final : public statement {
        private:
            std::unique_ptr<expression> value;

        public:
            explicit return_statement(std::unique_ptr<expression>&& return_value, source_location&& location)
                : ast_element(std::move(location)),
                value(std::move(return_value)) {}

            void build_c_string_indent(std::stringstream&, int) const override;

            std::unique_ptr<statement> clone_statement() const override {
                return std::make_unique<return_statement>(
                    value ? value->clone_expression() : nullptr,
                    source_location(location())
                );
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                if (value) {
                    value->visit(visitor);
                }
            }
        };

        class break_statement final : public statement {
        private:
            int loop_depth;

        public:
            explicit break_statement(int depth, source_location&& location)
                : ast_element(std::move(location)),
                loop_depth(depth) {}

            void build_c_string_indent(std::stringstream&, int) const override;

            std::unique_ptr<statement> clone_statement() const override {
                return std::make_unique<break_statement>(
                    loop_depth,
                    source_location(location())
                );
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
            }
        };

        class continue_statement final : public statement {
        private:
            int loop_depth;

        public:
            explicit continue_statement(int depth, source_location&& location)
                : ast_element(std::move(location)), loop_depth(depth) {}

            void build_c_string_indent(std::stringstream&, int) const override;

            std::unique_ptr<statement> clone_statement() const override {
                return std::make_unique<continue_statement>(
                    loop_depth,
                    source_location(location())
                );
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
            }
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
                m_value(value) {}

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

            std::unique_ptr<expression> clone_expression() const override {
                return std::make_unique<int_literal>(m_qualifiers, m_class, m_value, source_location(location()));
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
            }
        };

        class float_literal final : public expression {
        private:
            float m_value;

        public:
            explicit float_literal(float value, source_location&& location)
                : ast_element(std::move(location)),
                m_value(value) {}

            const float value() const noexcept {
                return m_value;
            }

            void build_c_string_prec(std::stringstream&, int) const override;

            std::unique_ptr<expression> clone_expression() const override {
                return std::make_unique<float_literal>(m_value, source_location(location()));
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
            }
        };

        class double_literal final : public expression {
        private:
            double m_value;

        public:
            explicit double_literal(double value, source_location&& location)
                : ast_element(std::move(location)),
                m_value(value) {}

            const double value() const noexcept {
                return m_value;
            }

            void build_c_string_prec(std::stringstream&, int) const override;

            std::unique_ptr<expression> clone_expression() const override {
                return std::make_unique<double_literal>(m_value, source_location(location()));
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
            }
        };

        class string_literal final : public expression {
        private:
            std::string m_value;
        public:
            explicit string_literal(std::string value, source_location&& location)
                : ast_element(std::move(location)), m_value(std::move(value)) {}

            const std::string& value() const noexcept { return m_value; }

            void build_c_string_prec(std::stringstream& ss, int parent_precedence) const override;

            std::unique_ptr<expression> clone_expression() const override {
                return std::make_unique<string_literal>(m_value, source_location(location()));
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
            }
        };

        class variable_reference final : public set_destination {
        private:
            std::string m_identifier;

        public:
            explicit variable_reference(std::string identifier, source_location&& location)
                : ast_element(std::move(location)),
                m_identifier(identifier) {}

            const std::string& identifier() const noexcept { return m_identifier; }

            void build_c_string_prec(std::stringstream&, int) const override;

            std::unique_ptr<set_destination> clone_set_destination() const override {
                return std::make_unique<variable_reference>(m_identifier, location());
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
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
                m_index(std::move(index)) {}

            void build_c_string_prec(std::stringstream&, int) const override;

            std::unique_ptr<set_destination> clone_set_destination() const override {
                return std::make_unique<get_index>(m_ptr->clone_set_destination(), m_index->clone_expression(), location());
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_ptr->visit(visitor);
                m_index->visit(visitor);
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
                m_is_pointer_dereference(is_pointer_dereference) {}

            void build_c_string_prec(std::stringstream&, int) const override;

            std::unique_ptr<set_destination> clone_set_destination() const override {
                return std::make_unique<get_property>(m_struct->clone_set_destination(), std::string(m_property_name), m_is_pointer_dereference, location());
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_struct->visit(visitor);
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
                m_set_value(std::move(set_value)) {}

            void build_c_string_prec(std::stringstream&, int) const override;
            void build_c_string_indent(std::stringstream&, int) const override;

            void build_c_string(std::stringstream& ss) const override {
                build_c_string_indent(ss, 0);
            }

            std::unique_ptr<set_operator> clone() const {
                return std::make_unique<set_operator>(m_set_dest->clone_set_destination(), m_set_value->clone_expression(), location());
            }

            std::unique_ptr<expression> clone_expression() const override {
                return clone();
            }

            std::unique_ptr<statement> clone_statement() const override {
                return clone();
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_set_dest->visit(visitor);
                m_set_value->visit(visitor);
            }
        };

        class dereference_operator final : public set_destination {
        private:
            std::unique_ptr<expression> m_pointer;

        public:
            explicit dereference_operator(std::unique_ptr<expression>&& pointer, source_location&& location)
                : ast_element(std::move(location)),
                m_pointer(std::move(pointer)) {}

            void build_c_string_prec(std::stringstream&, int) const override;

            std::unique_ptr<set_destination> clone_set_destination() const override {
                return std::make_unique<dereference_operator>(m_pointer->clone_expression(), location());
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_pointer->visit(visitor);
            }
        };

        class get_reference final : public expression {
        private:
            std::unique_ptr<expression> m_item;

        public:
            explicit get_reference(std::unique_ptr<expression>&& item, source_location&& location)
                : ast_element(std::move(location)),
                m_item(std::move(item)) {}

            void build_c_string_prec(std::stringstream&, int) const override;

            std::unique_ptr<expression> clone_expression() const override {
                return std::make_unique<get_reference>(m_item->clone_expression(), location());
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_item->visit(visitor);
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

            std::unique_ptr<expression> clone_expression() const override {
                return std::make_unique<arithmetic_operator>(m_operation, m_left->clone_expression(), m_right->clone_expression(), location());
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_left->visit(visitor);
                m_right->visit(visitor);
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

            std::unique_ptr<expression> clone_expression() const override {
                return std::make_unique<conditional_expression>(m_condition->clone_expression(), m_true_expr->clone_expression(), m_false_expr->clone_expression(), location());
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_condition->visit(visitor);
                m_true_expr->visit(visitor);
                m_false_expr->visit(visitor);
            }
        };

        class function_call : public expression, public statement {
        private:
            std::unique_ptr<expression> m_callee;
            std::vector<std::unique_ptr<expression>> m_arguments;

        public:
            function_call(std::unique_ptr<expression>&& callee,
                std::vector<std::unique_ptr<expression>>&& arguments,
                source_location&& location)
                : ast_element(std::move(location)),
                m_callee(std::move(callee)),
                m_arguments(std::move(arguments)) {}

            void build_c_string_prec(std::stringstream&, int) const override;
            void build_c_string_indent(std::stringstream&, int) const override;

            void build_c_string(std::stringstream& ss) const override {
                build_c_string_indent(ss, 0);
            }
            
            std::unique_ptr<function_call> clone() const {
                std::vector<std::unique_ptr<expression>> cloned_arguments;
                cloned_arguments.reserve(m_arguments.size());
                for (const auto& arg : m_arguments) {
                    cloned_arguments.emplace_back(arg->clone_expression());
                }
                return std::make_unique<function_call>(m_callee->clone_expression(), std::move(cloned_arguments), source_location(location()));
            }

            std::unique_ptr<expression> clone_expression() const override {
                return clone();
            }

            std::unique_ptr<statement> clone_statement() const override {
                return clone();
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_callee->visit(visitor);
                for (const auto& arg : m_arguments) {
                    arg->visit(visitor);
                }
            }
        };

        class initializer_list_expression : public expression {
        private:
            std::vector<std::unique_ptr<expression>> m_initializers;
        public:
            explicit initializer_list_expression(std::vector<std::unique_ptr<expression>>&& initializers, source_location&& location)
                : ast_element(std::move(location)), m_initializers(std::move(initializers)) {}

            const std::vector<std::unique_ptr<expression>>& initializers() const noexcept { return m_initializers; }

            void build_c_string_prec(std::stringstream& ss, int indent) const override;

            std::unique_ptr<expression> clone_expression() const override {
                std::vector<std::unique_ptr<expression>> cloned_initializers;
                cloned_initializers.reserve(m_initializers.size());
                for (const auto& init : m_initializers) {
                    cloned_initializers.emplace_back(init->clone_expression());
                }
                return std::make_unique<initializer_list_expression>(std::move(cloned_initializers), source_location(location()));
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                for (const auto& init : m_initializers) {
                    init->visit(visitor);
                }
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

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_type->visit(visitor);
                if (m_set_value.has_value()) {
                    m_set_value.value()->visit(visitor);
                }
            }

            variable_declaration clone() const {
                return variable_declaration(qualifiers(), type()->clone_type(), std::string(identifier()), location(), m_set_value ? std::make_optional(m_set_value.value()->clone_expression()) : std::nullopt);
            }

            std::unique_ptr<statement> clone_statement() const override {
                return std::make_unique<variable_declaration>(clone());
            }
        };

        class typedef_declaration final : public top_level_element {
        private:
            std::unique_ptr<type> m_type;
            std::string m_name;

        public:
            typedef_declaration(std::unique_ptr<type>&& type, std::string&& name, source_location&& location)
                : ast_element(std::move(location)),
                m_type(std::move(type)), m_name(std::move(name)) {}

            const type* type() const noexcept { return m_type.get(); }
            const std::string& name() const noexcept { return m_name; }

            void build_c_string(std::stringstream&) const override;

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_type->visit(visitor);
            }
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

            std::unique_ptr<type> clone_type() const override {
                if (m_struct_name.has_value()) {
                    return std::make_unique<struct_declaration>(std::optional<std::string>(m_struct_name), std::vector<variable_declaration>(), source_location(location()));
                }

                std::vector<variable_declaration> cloned_members;
                cloned_members.reserve(m_members.size());
                for (const variable_declaration& member : m_members) {
                    cloned_members.emplace_back(member.clone());
                }
                return std::make_unique<struct_declaration>(std::optional<std::string>(m_struct_name), std::move(cloned_members), source_location(location()));
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                for (const auto& member : m_members) {
                    member.visit(visitor);
                }
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

            std::unique_ptr<type> clone_type() const override {
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

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
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

            std::unique_ptr<type> clone_type() const override {
                if (m_union_name.has_value()) {
                    return std::make_unique<union_declaration>(std::optional<std::string>(m_union_name), std::vector<member>(), source_location(location()));
                }

                std::vector<member> cloned_members;
                cloned_members.reserve(m_members.size());
                for (const auto& m : m_members) {
                    cloned_members.emplace_back(m.member_type->clone_type(), std::string(m.member_name));
                }
                return std::make_unique<union_declaration>(std::optional<std::string>(m_union_name), std::move(cloned_members), source_location(location()));
            }

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                for (const auto& member : m_members) {
                    member.member_type->visit(visitor);
                }
            }
        };

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
                m_parameters(std::move(parameters)) {}

            const type* return_type() const noexcept { return m_return_type.get(); }
            const std::string& function_name() const noexcept { return m_function_name; }
            const std::vector<function_parameter>& parameters() const noexcept { return m_parameters; }

            void build_c_string(std::stringstream&) const override;

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_return_type->visit(visitor);
                for (const auto& param : m_parameters) {
                    param.param_type->visit(visitor);
                }
            }
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

            void visit(std::unique_ptr<visitor>& visitor) const override {
                visitor->visit(*this);
                m_return_type->visit(visitor);
                for (const auto& param : m_parameters) {
                    param.param_type->visit(visitor);
                }
                m_function_body.visit(visitor);
            }
        };

		class replacer_visitor {// Types
			virtual void visit(const int_type& node) { }
			virtual void visit(const float_type& node) { }
			virtual void visit(const pointer_type& node) { }
			virtual void visit(const array_type& node) { }
			virtual void visit(const function_pointer_type& node) { }

			// Statements
			virtual void visit(const context_block& node) { }
			virtual void visit(const for_loop& node) { }
			virtual void visit(const do_block& node) { }
			virtual void visit(const while_block& node) { }
			virtual void visit(const if_block& node) { }
			virtual void visit(const if_else_block& node) { }
			virtual void visit(const return_statement& node) { }
			virtual void visit(const break_statement& node) { }
			virtual void visit(const continue_statement& node) { }

			// Expressions
			virtual void visit(const int_literal& node) { }
			virtual void visit(const float_literal& node) { }
			virtual void visit(const double_literal& node) { }
			virtual void visit(const string_literal& node) { }
			virtual void visit(const variable_reference& node) { }
			virtual void visit(const get_index& node) { }
			virtual void visit(const get_property& node) { }
			virtual void visit(const set_operator& node) { }
			virtual void visit(const dereference_operator& node) { }
			virtual void visit(const get_reference& node) { }
			virtual void visit(const arithmetic_operator& node) { }
			virtual void visit(const conditional_expression& node) { }
			virtual void visit(const function_call& node) { }
			virtual void visit(const initializer_list_expression& node) { }

			// Top-level elements
			virtual void visit(const variable_declaration& node) { }
			virtual void visit(const typedef_declaration& node) { }
			virtual void visit(const struct_declaration& node) { }
			virtual void visit(const enum_declaration& node) { }
			virtual void visit(const union_declaration& node) { }
			virtual void visit(const function_prototype& node) { }
			virtual void visit(const function_declaration& node) { }
		};
	}
}