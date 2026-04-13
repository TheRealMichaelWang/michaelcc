#ifndef MICHAELCC_LINEAR_ALLOCATORS_REGISTER_ALLOCATOR_HPP
#define MICHAELCC_LINEAR_ALLOCATORS_REGISTER_ALLOCATOR_HPP

#include "linear/ir.hpp"
#include "linear/registers.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace michaelcc::linear::allocators {
    class register_allocator {
    private:
        struct block_info {
            std::unordered_set<virtual_register> defined_vregs;
            std::unordered_set<virtual_register> used_vregs;
        };

        //liveliness information is stored per block
        struct block_liveliness {
            std::unordered_set<virtual_register> live_in;
            std::unordered_set<virtual_register> live_out;
        };

        //inference graph node for graph coloring
        struct inference_graph_node {
            virtual_register vreg;
            std::unordered_set<virtual_register> adjacent_vregs;
            size_t degree;

            bool must_avoid_caller_saved = false;
        };

        translation_unit& m_translation_unit;
        
        // the inference graph
        std::unordered_map<virtual_register, inference_graph_node> m_inference_graph;
        //std::unordered_map<virtual_register, register_t> m_coloring;

        // the block liveliness information
        std::unordered_map<size_t, block_liveliness> m_block_liveliness;
        std::unordered_map<size_t, block_info> m_block_info;

        std::unordered_set<virtual_register> compute_defined_vregs(size_t block_id);
        std::unordered_set<virtual_register> compute_used_vregs(size_t block_id, const std::unordered_set<virtual_register>& defined_vregs);

        block_info& compute_block_info(size_t block_id) {
            if (m_block_info.contains(block_id)) {
                return m_block_info.at(block_id);
            }
            auto defined_vregs = compute_defined_vregs(block_id);
            auto used_vregs = compute_used_vregs(block_id, defined_vregs);
            m_block_info.insert({ block_id, block_info{ 
                std::move(defined_vregs), 
                std::move(used_vregs),
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

        inference_graph_node& ensure_node(virtual_register vreg);
        void add_edge(virtual_register vreg_a, virtual_register vreg_b);

        void build_inference_graph(size_t block_id);
        void build_inference_graph();

        // counts the number of available registers of a given class and size
        size_t count_available_registers(register_class reg_class, word_size word_size);

        // returns the select stack... which then needs to be passed into select for register selection
        std::vector<virtual_register> simplify();

        // assigns the registers to each vreg in the select stack; returns a list of spilled vregs
        std::vector<virtual_register> select(const std::vector<virtual_register>& select_stack);

    public:
        register_allocator(translation_unit& translation_unit) : m_translation_unit(translation_unit) {}
    };
}
#endif