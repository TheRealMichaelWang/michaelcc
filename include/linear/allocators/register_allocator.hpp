#ifndef MICHAELCC_LINEAR_ALLOCATORS_REGISTER_ALLOCATOR_HPP
#define MICHAELCC_LINEAR_ALLOCATORS_REGISTER_ALLOCATOR_HPP

#include "linear/ir.hpp"
#include "linear/registers.hpp"
#include <cstdint>
#include <optional>
#include <unordered_set>


namespace michaelcc::linear::allocators {
    // we do graph coloring; 1 color = 1 register family
    struct register_family {
        uint8_t family_id;
        register_class reg_class;
    };

    struct block_info {
        std::unordered_set<virtual_register> defined_vregs;
        std::unordered_set<virtual_register> used_vregs;
        std::vector<const phi_instruction*> phi_instructions;
    };

    //liveliness information is stored per block
    struct block_liveliness {
        std::unordered_set<virtual_register> live_in;
        std::unordered_set<virtual_register> live_out;
    };

    //inference graph node for graph coloring
    struct inference_graph_node {
        virtual_register vreg;
        std::optional<uint8_t> precolored_family;
        std::vector<uint8_t> adjacent_node_ids;
        size_t degree;
    };

    class register_allocator {
    private:
        translation_unit& m_translation_unit;
        
        // the inference graph
        std::unordered_map<size_t, inference_graph_node> m_inference_graph;

        // the block liveliness information
        std::unordered_map<size_t, block_liveliness> m_block_liveliness;
        std::unordered_map<size_t, block_info> m_block_info;

        std::unordered_set<virtual_register> compute_defined_vregs(size_t block_id);
        std::unordered_set<virtual_register> compute_used_vregs(size_t block_id, const std::unordered_set<virtual_register>& defined_vregs);
        std::vector<const phi_instruction*> compute_phi_instructions(size_t block_id);

        block_info& compute_block_info(size_t block_id) {
            if (m_block_info.contains(block_id)) {
                return m_block_info.at(block_id);
            }
            auto defined_vregs = compute_defined_vregs(block_id);
            auto used_vregs = compute_used_vregs(block_id, defined_vregs);
            auto phi_instructions = compute_phi_instructions(block_id);
            m_block_info.insert({ block_id, block_info{ 
                std::move(defined_vregs), 
                std::move(used_vregs), 
                std::move(phi_instructions) 
            } });
            return m_block_info.at(block_id);
        }

        template<typename T>
        static bool update_set(std::unordered_set<T>& set, const std::unordered_set<T>& other) {
            bool changed = false;
            for (const auto& value : other) {
                if (!set.contains(value)) {
                    set.insert(value);
                    changed = true;
                }
            }
            return changed;
        }

        bool compute_block_liveliness(size_t block_id);
        void compute_all_block_liveliness();

        void build_inference_graph();
    public:
        register_allocator(translation_unit& translation_unit) : m_translation_unit(translation_unit) {}
    };
}
#endif