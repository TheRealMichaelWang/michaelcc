#ifndef MICHAELCC_ISA_LC2200_HPP
#define MICHAELCC_ISA_LC2200_HPP

#include "isa.hpp"
#include "platform.hpp"
#include "assembly/assembler.hpp"
#include "linear/ir.hpp"
#include <memory>
#include <ostream>

namespace michaelcc::isa::lc2200 {
    class lc2200_assembler : public assembly::assembler {
    public:
        lc2200_assembler(std::ostream& output) : assembly::assembler(output) {}
        
    protected:
        void begin_block_preamble(const linear::basic_block& block) override;
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
    };

    class lc2200_isa : public isa {
    public:
        const platform_info& get_platform_info() const noexcept override;
        
        std::unique_ptr<assembly::assembler> create_assembler(std::ostream& output) const override { 
            return std::make_unique<lc2200_assembler>(output); 
        }
        
        void legalize(linear::translation_unit& unit) const override { }
    };
}

#endif