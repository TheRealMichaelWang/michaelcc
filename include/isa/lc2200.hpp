#ifndef MICHAELCC_ISA_LC2200_HPP
#define MICHAELCC_ISA_LC2200_HPP

#include "isa.hpp"
#include "linear/registers.hpp"
#include "platform.hpp"
#include "assembly/assembler.hpp"
#include "linear/ir.hpp"
#include <memory>
#include <ostream>
#include <unordered_set>

namespace michaelcc::isa::lc2200 {
    class lc2200_assembler : public assembly::assembler {
    private:
        struct function_call_info {
            // add each offset to sp to get the address of the caller saved register
            std::unordered_map<linear::register_t, size_t> caller_saved_registers_offsets;
            std::unordered_set<linear::register_t> trashed_registers;

            size_t pushed_parameter_size; // size for parameters on stack
            size_t pushed_register_size; // size for registers on stack
        };

        std::unordered_map<size_t, function_call_info> m_function_call_infos;
    
    public:
        lc2200_assembler(std::ostream& output) : assembly::assembler(output) {}
        
    protected:
        void begin_block_preamble(const linear::basic_block& block) override { }
        void begin_function_preamble(const linear::function_definition& definition) override;
        void begin_function_call(const linear::function_call& instruction) override;

        void dispatch(const linear::a_instruction& instruction) override;
        void dispatch(const linear::a2_instruction& instruction) override;
        void dispatch(const linear::u_instruction& instruction) override;
        void dispatch(const linear::c_instruction& instruction) override;
        void dispatch(const linear::init_register& instruction) override;
        void dispatch(const linear::load_parameter& instruction) override;
        void dispatch(const linear::load_effective_address& instruction) override;
        void dispatch(const linear::load_memory& instruction) override;
        void dispatch(const linear::store_memory& instruction) override;
        void dispatch(const linear::valloca_instruction& instruction) override;
        void dispatch(const linear::branch& instruction) override;
        void dispatch(const linear::branch_condition& instruction) override;
        void dispatch(const linear::push_function_argument& instruction) override;
        void dispatch(const linear::function_call& instruction) override;
        void dispatch(const linear::function_return& instruction) override;

        void dispatch(const linear::alloca_instruction& instruction) override { throw std::runtime_error("alloca instructions shouldve been removed by frame allocator."); }
        void dispatch(const linear::phi_instruction& instruction) override { throw std::runtime_error("phi instructions shouldve been removed."); }

    private:
        void assign_argument_registers(linear::translation_unit& unit, size_t function_id);
    };

    class lc2200_isa final : public isa {
    public:
        const platform_info& get_platform_info() const noexcept override;
        
        std::unique_ptr<assembly::assembler> create_assembler(std::ostream& output) const override { 
            return std::make_unique<lc2200_assembler>(output); 
        }

        void assign_parameter_registers(std::vector<linear::function_parameter>& parameters) override;
        void assign_argument_registers(std::vector<linear::function_argument>& arguments) override;
    };
}

#endif