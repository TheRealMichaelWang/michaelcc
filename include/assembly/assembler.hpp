#ifndef MICHAELCC_ASSEMBLY_ASSEMBLER_HPP
#define MICHAELCC_ASSEMBLY_ASSEMBLER_HPP

#include "linear/ir.hpp"
#include "linear/registers.hpp"
#include <stdexcept>

namespace michaelcc::assembly {
    class assembler: public linear::instruction_dispatcher<void> {
    private:
        bool m_skip_next_instruction;
        std::optional<const linear::instruction*> m_next_instruction;
        size_t m_symbol_counter;

    protected:
        std::ostream& m_output;
        std::optional<const linear::translation_unit*> m_current_unit;
    
    public:

        assembler(std::ostream& output) 
            : m_output(output), m_current_unit(std::nullopt), m_skip_next_instruction(false), m_next_instruction(std::nullopt), m_symbol_counter(0) {}

        virtual ~assembler() = default;

    private:
        void assemble_block(const linear::translation_unit& unit, size_t block_id, bool emit_label);
        void assemble_function(const linear::translation_unit& unit, size_t function_id);

    protected:
        void begin_new_line() {
            m_output << "\n\t";
        }

        std::string generate_symbol() {
            m_symbol_counter++;
            return "sym" + std::to_string(m_symbol_counter);
        }

        void emit_label(std::string label) {
            m_output << '\n' <<label << ":";
        }

        linear::register_info get_physical_register(const linear::virtual_register& vreg) const {
            return m_current_unit.value()->platform_info.get_register_info(m_current_unit.value()->vreg_colors.at(vreg));
        }

        std::optional<const linear::instruction*> next_instruction() const {
            return m_next_instruction;
        }

        void skip_next_instruction() {
            m_skip_next_instruction = true;
        }

        bool in_physical_family(linear::register_t id_a, std::string family_register_name) const {
            auto register_info = m_current_unit.value()->platform_info.get_register_info(id_a);
            for (auto mutually_exclusive_register : register_info.mutually_exclusive_registers) {
                if (m_current_unit.value()->platform_info.get_register_info(mutually_exclusive_register).name == family_register_name) {
                    return true;
                }
            }
            return false;
        }

        linear::register_info get_physical_of_size(linear::register_t id_a, linear::word_size size) const {
            auto register_info = m_current_unit.value()->platform_info.get_register_info(id_a);
            if (register_info.size == size) {
                return register_info;
            }
            for (auto mutually_exclusive_register : register_info.mutually_exclusive_registers) {
                auto mutually_exclusive_register_info = m_current_unit.value()->platform_info.get_register_info(mutually_exclusive_register);
                if (mutually_exclusive_register_info.size == size) {
                    return mutually_exclusive_register_info;
                }
            }
            throw std::runtime_error("No physical register of size " + std::to_string(size) + " found");
        }

        // use this to emit pre-amble for a block
        virtual void begin_block_preamble(const linear::basic_block& block) = 0;

        // use this to emit pre-amble for function
        virtual void begin_function_preamble(const linear::function_definition& definition) = 0;

        // use this to potentially save caller-saved registers for a function call
        virtual void begin_function_call(const linear::function_call& instruction) = 0;

        void dispatch(const linear::a_instruction& instruction) override = 0;
        void dispatch(const linear::a2_instruction& instruction) override = 0;
        void dispatch(const linear::u_instruction& instruction) override = 0;
        void dispatch(const linear::c_instruction& instruction) override = 0;
        void dispatch(const linear::init_register& instruction) override = 0;
        void dispatch(const linear::load_parameter& instruction) override = 0;
        void dispatch(const linear::load_effective_address& instruction) override = 0;
        void dispatch(const linear::load_memory& instruction) override = 0;
        void dispatch(const linear::store_memory& instruction) override = 0;
        void dispatch(const linear::alloca_instruction& instruction) override = 0;
        void dispatch(const linear::valloca_instruction& instruction) override = 0;
        void dispatch(const linear::branch& instruction) override = 0;
        void dispatch(const linear::branch_condition& instruction) override = 0;
        void dispatch(const linear::push_function_argument& instruction) override = 0;
        void dispatch(const linear::function_call& instruction) override = 0;
        void dispatch(const linear::function_return& instruction) override = 0;
        void dispatch(const linear::phi_instruction& instruction) override { throw std::runtime_error("phi instructions are not supported by the assembler"); }

    public:
        void assemble(const linear::translation_unit& unit);

        virtual void legalize(linear::translation_unit& unit) = 0;
    };
}

#endif