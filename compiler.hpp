#ifndef MICHAELCC_COMPILER_HPP
#define MICHAELCC_COMPILER_HPP

#include "ast.hpp"
#include "errors.hpp"
#include "logical.hpp"
#include "typing.hpp"
#include <memory>

namespace michaelcc {
    class compiler {
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

            void forward_declare_function(const std::string& function_name, const std::vector<ast::function_parameter>& parameters, const source_location& location);
        public:
            forward_declare_functions(compiler& compiler) : m_compiler(compiler) { }

            void visit(const ast::function_declaration& node) override {
                forward_declare_function(node.function_name(), node.parameters(), node.location());
            }

            void visit(const ast::function_prototype& node) override {
                forward_declare_function(node.function_name(), node.parameters(), node.location());
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
                    types.emplace_back(field.field_type.type());
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
            logical_ir::translation_unit& m_translation_unit;

        public:
            type_resolver(logical_ir::translation_unit& translation_unit) : m_translation_unit(translation_unit) { }

            typing::qual_type resolve_int_type(const ast::type_specifier& type);

        protected:
            typing::qual_type dispatch(const ast::type_specifier& type) override;
            typing::qual_type dispatch(const ast::qualified_type& type) override;
            typing::qual_type dispatch(const ast::derived_type& type) override;
            typing::qual_type dispatch(const ast::function_type& type) override;
            typing::qual_type dispatch(const ast::struct_declaration& type) override;
            typing::qual_type dispatch(const ast::union_declaration& type) override;
            typing::qual_type dispatch(const ast::enum_declaration& type) override;
        };

        compilation_error panic(const std::string& msg, const source_location& location) {
            return compilation_error(msg, location);
        }

        logical_ir::translation_unit m_translation_unit;
        const platform_info m_platform_info;

        layout_dependency_getter m_layout_dependency_getter;
        type_layout_calculator m_type_layout_calculator;

        std::map<std::shared_ptr<typing::base_type>, const source_location> m_type_declaration_locations;

        typing::qual_type resolve_type(const ast::ast_element& type);

        void check_layout_dependencies(std::shared_ptr<typing::base_type>& type);
        void calculate_type_sizes(std::shared_ptr<typing::base_type>& type);
    public:
        compiler(const platform_info platform_info) 
            : m_translation_unit(), m_platform_info(platform_info), 
            m_layout_dependency_getter(m_translation_unit), 
            m_type_layout_calculator(m_platform_info) { }
    };
}
#endif
