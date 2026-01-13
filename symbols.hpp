#ifndef MICHAELCC_SYMBOLS_HPP
#define MICHAELCC_SYMBOLS_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <vector>
#include <ranges>

namespace michaelcc {
    namespace logical_ir {
        class symbol;
        class symbol_context;

        class symbol {
        protected:
            std::string m_name;
            std::weak_ptr<symbol_context> m_context;

        public:
            explicit symbol(std::string&& name, std::weak_ptr<symbol_context>&& context) : m_name(std::move(name)), m_context(std::move(context)) {}
            virtual ~symbol() = default;

            const std::string& name() const noexcept { return m_name; }

            virtual std::string to_string() const noexcept = 0;

            void set_context(std::weak_ptr<symbol_context>&& context) { 
                if (auto existing = m_context.lock()) {
                    throw std::runtime_error("Context already set");
                }
                m_context = std::move(context); 
            }
        };

        class symbol_context {
        private:
            std::unordered_map<std::string, std::shared_ptr<symbol>> m_symbol_table;
            std::vector<std::shared_ptr<symbol>> m_symbols; 

        public:
            std::shared_ptr<symbol> lookup(const std::string& name) const {
                auto it = m_symbol_table.find(name);
                
                if (it != m_symbol_table.end()) {
                    return it->second;
                }

                return nullptr;
            }

            bool add(std::shared_ptr<symbol>&& sym) noexcept {
                if (m_symbol_table.contains(sym->name())) {
                    return false;
                }

                m_symbol_table[sym->name()] = sym;
                m_symbols.emplace_back(std::move(sym));
                return true;
            }

            const std::vector<std::shared_ptr<symbol>>& symbols() const noexcept { return m_symbols; }
        };

        class symbol_explorer {
        private:
            std::vector<std::shared_ptr<symbol_context>> m_contexts;

        public:
            void visit(std::shared_ptr<symbol_context> context) {
                m_contexts.push_back(context);
            }

            void exit() {
                m_contexts.pop_back();
            }

            std::shared_ptr<symbol> lookup(const std::string& name) const {
                for (const auto& context : std::views::reverse(m_contexts)) {
                    auto sym = context->lookup(name);
                    if (sym) {
                        return sym;
                    }
                }
                return nullptr;
            }

            bool add(std::shared_ptr<symbol>&& sym) noexcept {
                if (lookup(sym->name())) {
                    return false;
                }
                
                m_contexts.back()->add(std::move(sym));
                return true;
            }

            template<typename ContextType>
            std::shared_ptr<ContextType> is_in_context()
            {
                for (const auto& context : std::views::reverse(m_contexts)) {
                    auto context_ptr = std::dynamic_pointer_cast<ContextType>(context);
                    if (context_ptr) {
                        return context_ptr;
                    }
                }
                return nullptr;
            }
        };
    }
}

#endif