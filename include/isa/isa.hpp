#ifndef MICHAELCC_ISA_ISA_HPP
#define MICHAELCC_ISA_ISA_HPP

#include "assembly/assembler.hpp"
#include "platform.hpp"
#include <memory>

namespace michaelcc::isa {
    class isa {
    public:
        virtual ~isa() = default;

        virtual const platform_info& get_platform_info() const noexcept = 0;

        virtual std::unique_ptr<assembly::assembler> create_assembler(std::ostream& output) const = 0;

        virtual void assign_parameter_registers(std::vector<linear::function_parameter>& parameters) = 0;
        virtual void assign_argument_registers(std::vector<linear::function_argument>& arguments) = 0;
    };
}
#endif