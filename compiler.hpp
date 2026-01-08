#ifndef MICHAELCC_COMPILER_HPP
#define MICHAELCC_COMPILER_HPP

#include "ast.hpp"
#include "logical.hpp"
#include "typing.hpp"

namespace michaelcc {
    class compiler {
    private:
        logical_ir::translation_unit m_translation_unit;

        compilation_error panic(const std::string& msg, const source_location& location) {
            return compilation_error(msg, location);
        }

        class forward_declare_types final : public ast::visitor {
        private:
            compiler& m_compiler;

        public:
            forward_declare_types(compiler& linker) : m_compiler(linker) { }

            void visit(const ast::struct_declaration& node) override;

            void visit(const ast::union_declaration& node) override;

            void visit(const ast::enum_declaration& node) override;
        };

        class implement_type_declarations final : public ast::visitor {
        private:
            compiler& m_compiler;

        public:
            implement_type_declarations(compiler& linker) : m_compiler(linker) { }

            void visit(const ast::struct_declaration& node) override;

            void visit(const ast::union_declaration& node) override;
        };

        class forward_declare_functions final : public ast::visitor {
        private:
            compiler& m_compiler;

            void forward_declare_function(const std::string& function_name, const std::vector<ast::function_parameter>& parameters, const source_location& location);
        public:
            forward_declare_functions(compiler& linker) : m_compiler(linker) { }

            void visit(const ast::function_declaration& node) override {
                forward_declare_function(node.function_name(), node.parameters(), node.location());
            }

            void visit(const ast::function_prototype& node) override {
                forward_declare_function(node.function_name(), node.parameters(), node.location());
            }
        };

        std::unique_ptr<typing::type> resolve_type(const ast::ast_element& type);

        void calculate_type_sizes();
    public:
        compiler() : m_translation_unit() { }
    };
}
#endif
