#ifndef MICHAELCC_LINKER_HPP
#define MICHAELCC_LINKER_HPP

#include "ast.hpp"
#include "logical.hpp"
#include "typing.hpp"

namespace michaelcc {
    class linker {
    private:
        logical_ir::translation_unit& m_translation_unit;

        compilation_error panic(const std::string& msg, const source_location& location) {
            return compilation_error(msg, location);
        }

        class type_declare_pass final : public ast::visitor {
        private:
            linker& m_linker;

        public:
            type_declare_pass(linker& linker) : m_linker(linker) { }

            void visit(const ast::struct_declaration& node) override;

            void visit(const ast::union_declaration& node) override;

            void visit(const ast::enum_declaration& node) override;
        };

        class type_implement_pass final : public ast::visitor {
        private:
            linker& m_linker;

        public:
            type_implement_pass(linker& linker) : m_linker(linker) { }

            void visit(const ast::struct_declaration& node) override;

            void visit(const ast::union_declaration& node) override;
        };

        class function_declare_pass final : public ast::visitor {
        private:
            linker& m_linker;
        public:
            function_declare_pass(linker& linker) : m_linker(linker) { }

            void visit(const ast::function_declaration& node) override;

            void visit(const ast::function_prototype& node) override;
        };

        std::unique_ptr<typing::type> resolve_type(const ast::ast_element& type);
    public:
        linker(logical_ir::translation_unit& translation_unit) : m_translation_unit(translation_unit) { }
    };
}
#endif
