#include "compiler.hpp"
#include "typing.hpp"
#include <memory>
#include <queue>
#include <sstream>

using namespace michaelcc;

void compiler::resolve_layout_dependencies(std::shared_ptr<typing::type>& type) const {
    layout_dependency_getter resolver(m_translation_unit);
    
    std::map<std::shared_ptr<typing::type>, std::shared_ptr<typing::type>> last_seen_parent;

    std::queue<std::shared_ptr<typing::type>> type_to_resolve;
    type_to_resolve.push(type);

    while (!type_to_resolve.empty()) {
        std::shared_ptr<typing::type> type = type_to_resolve.front();
        if (last_seen_parent.contains(type)) { //check for circular dependencies
            std::ostringstream ss;

            ss << "Circular memory layout dependency detected with type " << type->to_string();
            if (m_type_declaration_locations.contains(type)) {
                ss << "(at " << m_type_declaration_locations.at(type).to_string() << ')';
            }

            std::vector<std::shared_ptr<typing::type>> path;
            std::shared_ptr<typing::type> current = last_seen_parent[type];
            while (current != type) {
                path.push_back(current);
                current = last_seen_parent[current];
            }
            path.push_back(type);
            std::reverse(path.begin(), path.end());
            ss << " in the following path: ";

            for (const auto& path_type : path) {
                ss << path_type->to_string();
                if (m_type_declaration_locations.contains(path_type)) {
                    ss << " (at " << m_type_declaration_locations.at(path_type).to_string() << ')';
                }
                if (path_type != type) {
                    ss << " -> ";
                }
            }
            ss << '.';

            throw compilation_error(ss.str(), m_type_declaration_locations.at(type));
        }
        type_to_resolve.pop();

        const std::vector<std::shared_ptr<typing::type>> dependencies = resolver.dispatch_dynamic(*type);
        for (const auto& dependency : dependencies) {
            type_to_resolve.push(dependency);
            last_seen_parent[dependency] = type;
        }
    }
}