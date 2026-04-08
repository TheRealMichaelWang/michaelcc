#include "linear/registers.hpp"
#include <stdexcept>

using namespace michaelcc::linear;

void register_allocator::set_alloc_information(virtual_register vreg, std::shared_ptr<alloc_information> alloc_information) {
    auto it = m_vreg_alloc_information.find(vreg.id);
    if (it != m_vreg_alloc_information.end()) {
        //merge alloc information
        if (alloc_information->must_use_register) {
            it->second->must_use_register = true;
        }
        if (alloc_information->register_id.has_value()) {
            if (it->second->register_id.has_value()) { 
                if (it->second->register_id.value() == alloc_information->register_id.value()) { return; }
                throw std::runtime_error("Cannot merge alloc information with different register ids"); 
            }
            it->second->register_id = alloc_information->register_id.value();
        }
    }
    else {
        m_vreg_alloc_information[vreg.id] = alloc_information;
    }
}