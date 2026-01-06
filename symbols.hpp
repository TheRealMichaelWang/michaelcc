#ifndef MICHAELCC_SYMBOLS_HPP
#define MICHAELCC_SYMBOLS_HPP

#include <string>
#include <unordered_map>
#include <memory>

namespace michaelcc {
    namespace logical_ir {
        class symbol {
        protected:
            std::string m_name;
        public:
            explicit symbol(std::string&& name) : m_name(std::move(name)) {}
            virtual ~symbol() = default;

            const std::string& name() const noexcept { return m_name; }
        };

        class symbol_context {
        private:
            std::unordered_map<std::string, std::unique_ptr<symbol>> m_symbol_table;

        public:
            explicit symbol_context() = default;

            symbol* lookup(const std::string& name) const {
                auto it = m_symbol_table.find(name);
                return it != m_symbol_table.end() ? it->second.get() : nullptr;
            }

            void add(std::unique_ptr<symbol>&& sym) {
                m_symbol_table[sym->name()] = std::move(sym);
            }
        };
    }
}

#endif