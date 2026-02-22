#ifndef MICHAELCC_CFG_IR_HPP
#define MICHAELCC_CFG_IR_HPP

#include "logic/ir.hpp"
#include "utils.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace michaelcc {
	namespace linear {
        struct virtual_register { 
            size_t id; size_t size_bits; 

            bool operator==(const virtual_register& reg) const { return reg.id == id; }
        };

        struct var_info {
            linear::virtual_register vreg;
            size_t block_id;

            bool operator==(const var_info& info) const { return info.vreg == vreg; }
        };

        class instruction {
        public:
            virtual ~instruction() = default;
        };

        class a_instruction;
        class a2_instruction;
        class init_register;
        class load_memory;
        class store_memory;
        class alloca_instruction;
        class valloca_instruction;
        class basic_block;
        class branch;
        class branch_condition;
        class function_call;
        class function_return;
        class phi_instruction;
        class load_effective_address;

        using instruction_transformer = generic_dispatcher<std::unique_ptr<instruction>, const instruction,
            const a_instruction,
            const a2_instruction,
            const init_register,
            const load_memory,
            const store_memory,
            const alloca_instruction,
            const valloca_instruction,
            const branch,
            const branch_condition,
            const function_call,
            const function_return,
            const phi_instruction,
            const load_effective_address
        >;

        // Arithmetic "A" instructions (includes comparison)
        enum a_instruction_type {
            MICHAELCC_LINEAR_A_ADD, MICHAELCC_LINEAR_A_SUBTRACT, MICHAELCC_LINEAR_A_MULTIPLY, MICHAELCC_LINEAR_A_DIVIDE, MICHAELCC_LINEAR_A_MODULO,

            MICHAELCC_LINEAR_A_SHIFT_LEFT, MICHAELCC_LINEAR_A_SHIFT_RIGHT, 
            MICHAELCC_LINEAR_A_BITWISE_AND, MICHAELCC_LINEAR_A_BITWISE_OR, MICHAELCC_LINEAR_A_BITWISE_XOR, MICHAELCC_LINEAR_A_BITWISE_NOT,
            
            MICHAELCC_LINEAR_A_AND, MICHAELCC_LINEAR_A_OR, MICHAELCC_LINEAR_A_XOR, MICHAELCC_LINEAR_A_NOT,

            MICHAELCC_LINEAR_A_COMPARE_EQUAL, MICHAELCC_LINEAR_A_COMPARE_NOT_EQUAL, 
            MICHAELCC_LINEAR_A_COMPARE_LESS_THAN, MICHAELCC_LINEAR_A_COMPARE_LESS_THAN_OR_EQUAL, 
            MICHAELCC_LINEAR_A_COMPARE_GREATER_THAN, MICHAELCC_LINEAR_A_COMPARE_GREATER_THAN_OR_EQUAL,
        };

        class a_instruction : public instruction {
        private:
            a_instruction_type m_type;
            virtual_register m_destination;
            
            virtual_register m_operand_a;
            virtual_register m_operand_b;

        public:
            a_instruction(a_instruction_type type, virtual_register destination, virtual_register operand_a, virtual_register operand_b) 
                : m_type(type), m_destination(destination), m_operand_a(operand_a), m_operand_b(operand_b) {}

            a_instruction_type type() const noexcept { return m_type; }
            virtual_register destination() const noexcept { return m_destination; }
            virtual_register operand_a() const noexcept { return m_operand_a; }
            virtual_register operand_b() const noexcept { return m_operand_b; }
        };


        // Arithmetic "A2" instructions (includes multiplication by a constant)
        class a2_instruction : public instruction {
        private:
            a_instruction_type m_type;
            virtual_register m_destination;
            virtual_register m_operand_a;

            size_t m_constant;  
        public:
            a2_instruction(a_instruction_type type, virtual_register destination, virtual_register operand_a, size_t constant) 
            : m_type(type), m_destination(destination), m_operand_a(operand_a), m_constant(constant) {}

            a_instruction_type type() const noexcept { return m_type; }
            virtual_register destination() const noexcept { return m_destination; }
            virtual_register operand_a() const noexcept { return m_operand_a; }
            size_t constant() const noexcept { return m_constant; }
        };


        // Unary "U" instructions
        enum u_instruction_type {
            MICHAELCC_LINEAR_U_NEGATE,
            MICHAELCC_LINEAR_U_NOT,
            MICHAELCC_LINEAR_U_BITWISE_NOT,
        };

        class u_instruction : public instruction {
        private:
            u_instruction_type m_type;
            virtual_register m_destination;
            virtual_register m_operand;
        public:
            u_instruction(u_instruction_type type, virtual_register destination, virtual_register operand) 
                : m_type(type), m_destination(destination), m_operand(operand) {}
            
            u_instruction_type type() const noexcept { return m_type; }
            virtual_register destination() const noexcept { return m_destination; }
            virtual_register operand() const noexcept { return m_operand; }
        };

        
        // Initialize a register with a literal value
        class init_register : public instruction {
        private:
            virtual_register m_destination;
            uint64_t m_value;
        public:
            init_register(virtual_register destination, uint64_t value) : m_destination(destination), m_value(value) {}
            virtual_register destination() const noexcept { return m_destination; }
            uint64_t value() const noexcept { return m_value; }
        };


        // Load from memory
        class load_memory : public instruction {
        private:
            virtual_register m_destination;
            virtual_register m_source_address;

            int64_t m_offset;
            size_t m_size_bytes;
        public:
            load_memory(virtual_register destination, virtual_register source_address, int64_t offset, size_t size_bytes) 
                : m_destination(destination), m_source_address(source_address), m_offset(offset), m_size_bytes(size_bytes) {}
        
            virtual_register destination() const noexcept { return m_destination; }
            virtual_register source_address() const noexcept { return m_source_address; }
            int64_t offset() const noexcept { return m_offset; }
            size_t size_bytes() const noexcept { return m_size_bytes; }
        };


        // Store to memory
        class store_memory : public instruction {
        private:
            virtual_register m_source_address;
            virtual_register m_value;

            int64_t m_offset;
            size_t m_size_bytes;
        public:
            store_memory(virtual_register source_address, virtual_register value, int64_t offset, size_t size_bytes) 
                : m_source_address(source_address), m_value(value), m_offset(offset), m_size_bytes(size_bytes) {}
        
            virtual_register source_address() const noexcept { return m_source_address; }
            virtual_register value() const noexcept { return m_value; }
            int64_t offset() const noexcept { return m_offset; }
            size_t size_bytes() const noexcept { return m_size_bytes; }
        };


        // Allocate memory on the stack
        class alloca_instruction : public instruction {
        private:
            virtual_register m_destination;
            size_t m_size_bytes;
            size_t m_alignment;

        public:
            alloca_instruction(virtual_register destination, size_t size_bytes, size_t alignment)
                : m_destination(destination), m_size_bytes(size_bytes), m_alignment(alignment) {}

            virtual_register destination() const noexcept { return m_destination; }
            size_t size_bytes() const noexcept { return m_size_bytes; }
            size_t alignment() const noexcept { return m_alignment; }
        };

        class valloca_instruction : public instruction {
        private:
            virtual_register m_destination;
            virtual_register m_size;
            size_t m_alignment;

        public:
            valloca_instruction(virtual_register destination, virtual_register size, size_t alignment)
                : m_destination(destination), m_size(size), m_alignment(alignment) {}

            virtual_register destination() const noexcept { return m_destination; }
            virtual_register size() const noexcept { return m_size; }
            size_t alignment() const noexcept { return m_alignment; }
        };


        // Branch "B" instructions
        class basic_block {
        private:
            std::vector<std::unique_ptr<instruction>> m_instructions;
            size_t m_id;

        public:
            basic_block(size_t id, std::vector<std::unique_ptr<instruction>>&& instructions) 
                : m_id(id), m_instructions(std::move(instructions)) {}

            size_t id() const noexcept { return m_id; }

            const std::vector<std::unique_ptr<instruction>>& instructions() const noexcept { return m_instructions; }   
        };

        class branch : public instruction {
        private:
            size_t m_next_block_id;

        public:
            branch(size_t next_block_id) : m_next_block_id(next_block_id) {}

            size_t next_block_id() const noexcept { return m_next_block_id; }
        };

        class branch_condition : public instruction {
        private:
            virtual_register m_condition;
            size_t m_if_true_block_id;
            size_t m_if_false_block_id;
            bool m_is_loop; 
        public:
            branch_condition(virtual_register condition, size_t if_true_block_id, size_t if_false_block_id, bool is_loop) 
                : m_condition(condition), m_if_true_block_id(if_true_block_id), m_if_false_block_id(if_false_block_id), m_is_loop(is_loop) {}

            virtual_register condition() const noexcept { return m_condition; }
            size_t if_true_block_id() const noexcept { return m_if_true_block_id; }
            size_t if_false_block_id() const noexcept { return m_if_false_block_id; }
            bool is_loop() const noexcept { return m_is_loop; }
        };


        // Function Stuff
        class function_definition {
        private:
            std::string m_name;

            size_t m_entry_block_id;
            std::vector<virtual_register> m_parameters;

        public:
            function_definition(std::string name, size_t entry_block_id, std::vector<virtual_register>&& parameters) 
                : m_name(name), m_entry_block_id(entry_block_id), m_parameters(std::move(parameters)) {}
            
            size_t entry_block_id() const noexcept { return m_entry_block_id; }
            const std::vector<virtual_register>& parameters() const noexcept { return m_parameters; }
            const std::string& name() const noexcept { return m_name; }
        };


        class function_call : public instruction {
        public:
            using callable = std::variant<std::string, virtual_register>;
        
        private:
            virtual_register m_destination;
            callable m_callee;
            std::vector<virtual_register> m_arguments;

        public:
            function_call(virtual_register destination, callable&& callee, std::vector<virtual_register>&& arguments) 
                : m_destination(destination), m_callee(std::move(callee)), m_arguments(std::move(arguments)) {}

            virtual_register destination() const noexcept { return m_destination; }
            const callable& callee() const noexcept { return m_callee; }
            const std::vector<virtual_register>& arguments() const noexcept { return m_arguments; }
        };

        class function_return : public instruction {
        public:
            function_return() { } 
        };


        // Phi Instruction
        class phi_instruction : public instruction {
        private:
            virtual_register m_destination;
            std::vector<var_info> m_values;

        public:
            phi_instruction(virtual_register destination, std::vector<var_info>&& values) 
                : m_destination(destination), m_values(std::move(values)) {}

            virtual_register destination() const noexcept { return m_destination; }
            const std::vector<var_info>& values() const noexcept { return m_values; }

            void augment_values(const std::vector<var_info>& values) {
                for (const auto& value : values) {
                    if (std::find(m_values.begin(), m_values.end(), value) == m_values.end()) {
                        m_values.push_back(value);
                    }
                }
            }
        };

        
        // Load Effective Address / loads address of a global label
        class load_effective_address : public instruction {
        private:
            virtual_register m_destination;
            std::string m_label;

        public:
            load_effective_address(virtual_register destination, std::string label) 
                : m_destination(destination), m_label(label) {}

            virtual_register destination() const noexcept { return m_destination; }
            const std::string& label() const noexcept { return m_label; }
        };
	}
}

#endif
