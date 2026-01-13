#ifndef MICHAELCC_COMPILER_HPP
#define MICHAELCC_COMPILER_HPP

#include "ast.hpp"
#include "errors.hpp"
#include "logical.hpp"
#include "typing.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace michaelcc {
    class compiler {
    private:
        enum class arbitrate_numeric_type {
            NO,
            YES
        };
    public:
        struct platform_info {
            const size_t m_pointer_size;

            const size_t m_int_size;
            const size_t m_short_size;
            const size_t m_long_size;
            const size_t m_long_long_size;

            const size_t m_float_size;
            const size_t m_double_size;

            const size_t m_default_alignment;
            const size_t m_max_alignment;

            const bool optimize_struct_layout = true;
        };
    private:
        class forward_declare_types final : public ast::visitor {
        private:
            compiler& m_compiler;

        public:
            forward_declare_types(compiler& compiler) : m_compiler(compiler) { }

            void visit(const ast::struct_declaration& node) override;

            void visit(const ast::union_declaration& node) override;

            void visit(const ast::enum_declaration& node) override;
        };

        class implement_type_declarations final : public ast::visitor {
        private:
            compiler& m_compiler;

        public:
            implement_type_declarations(compiler& compiler) : m_compiler(compiler) { }

            void visit(const ast::struct_declaration& node) override;

            void visit(const ast::union_declaration& node) override;
        };

        class forward_declare_functions final : public ast::visitor {
        private:
            compiler& m_compiler;

            void forward_declare_function(const std::string& function_name, const ast::ast_element& return_type, const std::vector<ast::function_parameter>& parameters, const source_location& location);
        public:
            forward_declare_functions(compiler& compiler) : m_compiler(compiler) { }

            void visit(const ast::function_declaration& node) override {
                forward_declare_function(node.function_name(), *node.return_type(), node.parameters(), node.location());
            }

            void visit(const ast::function_prototype& node) override {
                forward_declare_function(node.function_name(), *node.return_type(), node.parameters(), node.location());
            }
        };

        class layout_dependency_getter final : public typing::type_dispatcher<const std::vector<std::shared_ptr<typing::base_type>>> {
        private:
            const logical_ir::translation_unit& m_translation_unit;

        public:
            layout_dependency_getter(const logical_ir::translation_unit& translation_unit) : m_translation_unit(translation_unit) { }

        protected:
            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::void_type& type) override { 
                return std::vector<std::shared_ptr<typing::base_type>>(); 
            }

            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::int_type& type) override { 
                return std::vector<std::shared_ptr<typing::base_type>>(); 
            }

            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::float_type& type) override { 
                return std::vector<std::shared_ptr<typing::base_type>>(); 
            }

            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::pointer_type& type) override { 
                return std::vector<std::shared_ptr<typing::base_type>>(); 
            }

            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::array_type& type) override { 
                return { type.element_type().type() };
            } 

            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::enum_type& type) override { 
                return std::vector<std::shared_ptr<typing::base_type>>(); 
            }

            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::function_pointer_type& type) override { 
                return { };
            }

            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::struct_type& type) override { 
                std::vector<std::shared_ptr<typing::base_type>> types;
                types.reserve(type.fields().size());
                for (const auto& field : type.fields()) {
                    types.emplace_back(field.member_type.type());
                }
                return types;
            }

            const std::vector<std::shared_ptr<typing::base_type>> dispatch(typing::union_type& type) override { 
                std::vector<std::shared_ptr<typing::base_type>> types;
                types.reserve(type.members().size());
                for (const auto& member : type.members()) {
                    types.emplace_back(member.member_type.type());
                }
                return types;
            }
        };

        struct type_layout_info {
            const size_t size;
            const size_t alignment;
        };

        class type_layout_calculator final : public typing::type_dispatcher<const type_layout_info> {
        private:
            std::map<const typing::base_type*, type_layout_info> m_declared_info;
            const platform_info& m_platform_info;

            const type_layout_info pointer_layout = {
                .size=m_platform_info.m_pointer_size,
                .alignment=std::min<size_t>(m_platform_info.m_pointer_size, m_platform_info.m_max_alignment)
            };

            const type_layout_info int_layout = {
                .size=m_platform_info.m_int_size,
                .alignment=std::min<size_t>(m_platform_info.m_int_size, m_platform_info.m_max_alignment)
            };

        public:
            type_layout_calculator(const platform_info& platform_info) : m_platform_info(platform_info) { }

        protected:
            const type_layout_info dispatch(typing::void_type& type) override {
                throw std::runtime_error("Void type is not a valid type for layout calculation");
            }

            const type_layout_info dispatch(typing::int_type& type) override;

            const type_layout_info dispatch(typing::float_type& type) override;

            const type_layout_info dispatch(typing::pointer_type& type) override { return pointer_layout; }

            const type_layout_info dispatch(typing::array_type& type) override { return pointer_layout; }

            const type_layout_info dispatch(typing::enum_type& type) override { return int_layout; }

            const type_layout_info dispatch(typing::function_pointer_type& type) override { return pointer_layout; }

            const type_layout_info dispatch(typing::struct_type& type) override;
            const type_layout_info dispatch(typing::union_type& type) override;
        };

        class type_resolver final : public ast::const_type_dispatcher<typing::qual_type> {
        private:
            compiler& m_compiler;

        public:
            type_resolver(compiler& compiler) : m_compiler(compiler) { }

            typing::qual_type resolve_int_type(const ast::type_specifier& type);

        protected:
            typing::qual_type dispatch(const ast::type_specifier& type) override;
            typing::qual_type dispatch(const ast::qualified_type& type) override;
            typing::qual_type dispatch(const ast::derived_type& type) override;
            typing::qual_type dispatch(const ast::function_type& type) override;
            typing::qual_type dispatch(const ast::struct_declaration& type) override;
            typing::qual_type dispatch(const ast::union_declaration& type) override;
            typing::qual_type dispatch(const ast::enum_declaration& type) override;

            void handle_default(const ast::ast_element& node) override;
        };


        class address_of_compiler : public ast::const_expression_dispatcher<std::unique_ptr<logical_ir::expression>> {
        private:
            compiler& m_compiler;

        public:
            address_of_compiler(compiler& compiler) : m_compiler(compiler) { }

            std::unique_ptr<logical_ir::expression> dispatch(const ast::int_literal& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::float_literal& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::double_literal& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::string_literal& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::variable_reference& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::get_index& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::get_property& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::set_operator& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::dereference_operator& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::get_reference& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::arithmetic_operator& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::conditional_expression& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::function_call& node) override { handle_default(node); return nullptr; }
            std::unique_ptr<logical_ir::expression> dispatch(const ast::initializer_list_expression& node) override { handle_default(node); return nullptr; }

            void handle_default(const ast::ast_element& node) override;
        };

        class expression_compiler : public ast::const_expression_dispatcher<std::unique_ptr<logical_ir::expression>> {
        private:
            std::optional<typing::qual_type> m_target_type;
            compiler& m_compiler;

        public:
            expression_compiler(compiler& compiler, std::optional<typing::qual_type> target_type = std::nullopt) : m_target_type(target_type), m_compiler(compiler) { }

            std::unique_ptr<logical_ir::expression> dispatch(const ast::int_literal& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::float_literal& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::double_literal& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::string_literal& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::variable_reference& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::get_index& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::get_property& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::set_operator& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::dereference_operator& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::get_reference& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::arithmetic_operator& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::conditional_expression& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::function_call& node) override;
            std::unique_ptr<logical_ir::expression> dispatch(const ast::initializer_list_expression& node) override;

            void handle_default(const ast::ast_element& node) override;
        };

        class statement_compiler : public ast::const_statement_dispatcher<std::unique_ptr<logical_ir::statement>> {
        private:
            compiler& m_compiler;
            int m_current_loop_depth;

        public:
            statement_compiler(compiler& compiler) : m_compiler(compiler), m_current_loop_depth(0) { }

        protected:
            std::unique_ptr<logical_ir::statement> dispatch(const ast::context_block& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::for_loop& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::do_block& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::while_block& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::if_block& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::if_else_block& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::return_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::break_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::continue_statement& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::set_operator& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::function_call& node) override;
            std::unique_ptr<logical_ir::statement> dispatch(const ast::variable_declaration& node) override;

            void handle_default(const ast::ast_element& node) override;
        };

        static compilation_error panic(const std::string& msg, const source_location& location) noexcept {
            return compilation_error(msg, location);
        }

        logical_ir::translation_unit m_translation_unit;
        const platform_info m_platform_info;

        layout_dependency_getter m_layout_dependency_getter;
        type_layout_calculator m_type_layout_calculator;
        type_resolver m_type_resolver;
        address_of_compiler m_address_of_compiler;

        logical_ir::symbol_explorer m_symbol_explorer;

        std::map<std::shared_ptr<typing::base_type>, const source_location> m_type_declaration_locations;

        void check_layout_dependencies(std::shared_ptr<typing::base_type>& type);

        bool is_index_type(const typing::qual_type& type) const noexcept {
            return typeid(type.type()) == typeid(typing::int_type);
        }

        bool is_indexable_type(const typing::qual_type& type) const noexcept {
            return typeid(type.type()) == typeid(typing::array_type) || typeid(type.type()) == typeid(typing::pointer_type);
        }

        bool is_numeric_type(const typing::qual_type& type) const noexcept {
            return typeid(type.type()) == typeid(typing::int_type) || typeid(type.type()) == typeid(typing::float_type);
        }

        std::optional<typing::qual_type> arbitrate_types(const typing::qual_type& left, const typing::qual_type& right, bool arbitrate_numeric=false) const noexcept;
        std::unique_ptr<logical_ir::expression> compile_expression(const ast::ast_element& node, std::optional<typing::qual_type> target_type = std::nullopt);
    public:
        compiler(const platform_info platform_info) 
            : m_translation_unit(), m_platform_info(platform_info), 
            m_layout_dependency_getter(m_translation_unit), 
            m_type_layout_calculator(m_platform_info),
            m_type_resolver(*this),
            m_address_of_compiler(*this),
            m_symbol_explorer() { }
    };
}
#endif
