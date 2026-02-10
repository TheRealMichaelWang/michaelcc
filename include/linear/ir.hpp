#ifndef MICHAELCC_CFG_IR_HPP
#define MICHAELCC_CFG_IR_HPP

#include "utils.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace michaelcc {
	namespace linear {
        struct virtual_register { size_t id; size_t size_bits; };
        struct literal { uint64_t value; size_t size_bits; };

        using operand = std::variant<virtual_register, literal>;

        inline size_t operand_size_bits(const operand& op) {
            return std::visit([](auto&& v) { return v.size_bits; }, op);
        }

        class instruction {
        public:
            virtual ~instruction() = default;
        };

        class a_instruction;
        class m_instruction;
        class alloca_instruction;
        class copy_instruction;
        class function_param;
        class global_address;
        class branch;
        class branch_condition;
        class phi_instruction;
        class function_call;
        class function_return;

        // Instruction dispatchers

        template<typename ReturnType>
        using instruction_dispatcher = generic_dispatcher<ReturnType, instruction,
            a_instruction,
            m_instruction,
            alloca_instruction,
            copy_instruction,
            function_param,
            global_address,
            branch,
            branch_condition,
            phi_instruction,
            function_call,
            function_return
        >;

        template<typename ReturnType>
        using const_instruction_dispatcher = generic_dispatcher<ReturnType, const instruction,
            const a_instruction,
            const m_instruction,
            const alloca_instruction,
            const copy_instruction,
            const function_param,
            const global_address,
            const branch,
            const branch_condition,
            const phi_instruction,
            const function_call,
            const function_return
        >;

        // Arithmetic "A" instructions (includes comparison)

        enum a_instruction_type {
            ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO,

            SHIFT_LEFT, SHIFT_RIGHT, 
            BITWISE_AND, BITWISE_OR, BITWISE_XOR, BITWISE_NOT,
            
            AND, OR, XOR, NOT,

            COMPARE_EQUAL, COMPARE_NOT_EQUAL, 
            COMPARE_LESS_THAN, COMPARE_LESS_THAN_OR_EQUAL, 
            COMPARE_GREATER_THAN, COMPARE_GREATER_THAN_OR_EQUAL,
        };

        class a_instruction : public instruction {
        private:
            a_instruction_type m_type;
            virtual_register m_destination;
            
            operand m_operand_a;
            operand m_operand_b;

        public:
            a_instruction(a_instruction_type type, virtual_register destination, operand operand_a, operand operand_b) 
                : m_type(type), m_destination(destination), m_operand_a(operand_a), m_operand_b(operand_b) {}

            a_instruction_type type() const noexcept { return m_type; }
            virtual_register destination() const noexcept { return m_destination; }
            operand operand_a() const noexcept { return m_operand_a; }
            operand operand_b() const noexcept { return m_operand_b; }
        };


        // Memory "M" instructions
        //   LOAD:  destination = MEM[source + offset]    (reads size_bits of destination)
        //   STORE: MEM[destination + offset] = source    (writes size_bits of source)

        enum m_instruction_type {
            LOAD,
            STORE,
        };

        class m_instruction : public instruction {
        private:
            m_instruction_type m_type;
            virtual_register m_destination;
            virtual_register m_source;
            size_t m_offset;

        public:
            m_instruction(m_instruction_type type, virtual_register destination, virtual_register source, size_t offset) 
                : m_type(type), m_destination(destination), m_source(source), m_offset(offset) {}

            m_instruction_type type() const noexcept { return m_type; }

            virtual_register destination() const noexcept { return m_destination; }
            virtual_register source() const noexcept { return m_source; }
            size_t offset() const noexcept { return m_offset; }
        };


        // Stack allocation
        //   destination vreg receives a pointer to the allocated stack space.

        class alloca_instruction : public instruction {
        private:
            virtual_register m_destination;
            size_t m_size_bytes;

        public:
            alloca_instruction(virtual_register destination, size_t size_bytes)
                : m_destination(destination), m_size_bytes(size_bytes) {}

            virtual_register destination() const noexcept { return m_destination; }
            size_t size_bytes() const noexcept { return m_size_bytes; }
        };


        // Copy/move (operand to vreg)

        class copy_instruction : public instruction {
        private:
            virtual_register m_destination;
            operand m_source;

        public:
            copy_instruction(virtual_register destination, operand source)
                : m_destination(destination), m_source(source) {}

            virtual_register destination() const noexcept { return m_destination; }
            operand source() const noexcept { return m_source; }
        };


        // Function parameter pseudo-instruction
        // Materialises the value of function parameter #index into destination.
        // Lowered to the platform ABI later.

        class function_param : public instruction {
        private:
            virtual_register m_destination;
            size_t m_param_index;

        public:
            function_param(virtual_register destination, size_t param_index)
                : m_destination(destination), m_param_index(param_index) {}

            virtual_register destination() const noexcept { return m_destination; }
            size_t param_index() const noexcept { return m_param_index; }
        };


        // Global / string address pseudo-instruction
        // Loads the address of a named global symbol into destination.

        class global_address : public instruction {
        private:
            virtual_register m_destination;
            std::string m_symbol_name;

        public:
            global_address(virtual_register destination, std::string symbol_name)
                : m_destination(destination), m_symbol_name(std::move(symbol_name)) {}

            virtual_register destination() const noexcept { return m_destination; }
            const std::string& symbol_name() const noexcept { return m_symbol_name; }
        };


        // Basic block

        class basic_block {
        private:
            size_t m_id;
            std::vector<std::unique_ptr<instruction>> m_instructions;

        public:
            basic_block() : m_id(0) {}

            explicit basic_block(size_t id, std::vector<std::unique_ptr<instruction>>&& instructions = {}) 
                : m_id(id), m_instructions(std::move(instructions)) {}

            basic_block(basic_block&&) = default;
            basic_block& operator=(basic_block&&) = default;

            size_t id() const noexcept { return m_id; }
            const std::vector<std::unique_ptr<instruction>>& instructions() const noexcept { return m_instructions; }
        };


        // Branch instructions

        class branch : public instruction {
        private:
            size_t m_target_block_id;

        public:
            explicit branch(size_t target_block_id) : m_target_block_id(target_block_id) {}

            size_t target_block_id() const noexcept { return m_target_block_id; }
        };

        class branch_condition : public instruction {
        private:
            operand m_condition;
            size_t m_if_true_block_id;
            size_t m_if_false_block_id;

        public:
            branch_condition(operand condition, size_t if_true_block_id, size_t if_false_block_id) 
                : m_condition(condition), m_if_true_block_id(if_true_block_id), m_if_false_block_id(if_false_block_id) {}

            operand condition() const noexcept { return m_condition; }
            size_t if_true_block_id() const noexcept { return m_if_true_block_id; }
            size_t if_false_block_id() const noexcept { return m_if_false_block_id; }
        };


        // Phi instruction

        class phi_instruction : public instruction {
        private:
            virtual_register m_destination;
            std::vector<std::pair<size_t, operand>> m_values; // (predecessor_block_id, value)

        public:
            phi_instruction(virtual_register destination, std::vector<std::pair<size_t, operand>>&& values) 
                : m_destination(destination), m_values(std::move(values)) {}

            virtual_register destination() const noexcept { return m_destination; }
            const std::vector<std::pair<size_t, operand>>& values() const noexcept { return m_values; }
        };


        // Function call
        // Callee is either a function name (direct call) or a vreg (indirect call).

        class function_call : public instruction {
        public:
            using callee_type = std::variant<std::string, virtual_register>;

        private:
            std::optional<virtual_register> m_destination;
            callee_type m_callee;
            std::vector<operand> m_arguments;

        public:
            function_call(std::optional<virtual_register> destination, callee_type callee, std::vector<operand>&& arguments) 
                : m_destination(destination), m_callee(std::move(callee)), m_arguments(std::move(arguments)) {}

            std::optional<virtual_register> destination() const noexcept { return m_destination; }
            const callee_type& callee() const noexcept { return m_callee; }
            const std::vector<operand>& arguments() const noexcept { return m_arguments; }
        };


        // Function return

        class function_return : public instruction {
        private:
            std::optional<operand> m_value;

        public:
            explicit function_return(std::optional<operand>&& value) 
                : m_value(std::move(value)) {}

            const std::optional<operand>& value() const noexcept { return m_value; }
        };


        

        // Function definition (holds all blocks for one function)

        class function_definition {
        private:
            std::string m_name;
            size_t m_entry_block_id;
            std::unordered_map<size_t, basic_block> m_blocks;
            size_t m_vreg_count;

        public:
            function_definition() : m_entry_block_id(0), m_vreg_count(0) {}

            function_definition(std::string name, size_t entry_block_id,
                std::unordered_map<size_t, basic_block>&& blocks, size_t vreg_count) 
                : m_name(std::move(name)), m_entry_block_id(entry_block_id),
                  m_blocks(std::move(blocks)), m_vreg_count(vreg_count) {}

            function_definition(function_definition&&) = default;
            function_definition& operator=(function_definition&&) = default;

            const std::string& name() const noexcept { return m_name; }
            size_t entry_block_id() const noexcept { return m_entry_block_id; }
            const std::unordered_map<size_t, basic_block>& blocks() const noexcept { return m_blocks; }
            size_t vreg_count() const noexcept { return m_vreg_count; }
        };


        // Program (output of logic lowerer)

        struct program {
            std::vector<function_definition> functions;
            std::vector<std::string> string_constants;
        };

        std::string to_string(const program& program);
	}
}

#endif
