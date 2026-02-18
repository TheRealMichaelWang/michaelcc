#ifndef MICHAELCC_CFG_IR_HPP
#define MICHAELCC_CFG_IR_HPP

#include "logic/ir.hpp"
#include <cstddef>

namespace michaelcc {
	namespace linear {
        struct virtual_register { 
            size_t id; size_t size_bits; 

            bool operator==(const virtual_register& reg) const { return reg.id == id; }
        };

        struct literal { 
            uint64_t value; size_t size_bits; 

            bool operator==(const literal& lit) const { return lit.value == value && lit.size_bits == size_bits; }
        };

        struct var_info {
            linear::virtual_register vreg;
            size_t block_id;

            bool operator==(const var_info& info) const { return info.vreg == vreg; }
        };

        using operand = std::variant<virtual_register, literal>;

        inline size_t get_operand_size(const operand& op) noexcept {
            return std::visit(overloaded{
                [](const virtual_register& vreg) noexcept -> size_t { return vreg.size_bits; },
                [](const literal& lit) noexcept -> size_t { return lit.size_bits; }
            }, op);
        }

        class instruction {
        public:
            virtual ~instruction() = default;
        };


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

        class a2_instruction : public instruction {
        private:
            a_instruction_type m_type;
            virtual_register m_destination;
            operand m_operand_a;

            size_t m_constant;  
        public:
            a2_instruction(a_instruction_type type, virtual_register destination, operand operand_a, size_t constant) 
            : m_type(type), m_destination(destination), m_operand_a(operand_a), m_constant(constant) {}

            a_instruction_type type() const noexcept { return m_type; }
            virtual_register destination() const noexcept { return m_destination; }
            operand operand_a() const noexcept { return m_operand_a; }
            size_t constant() const noexcept { return m_constant; }
        };

        class init_zero : public instruction {
        private:
            virtual_register m_destination;
            
        public:
            init_zero(virtual_register destination) : m_destination(destination) {}
            virtual_register destination() const noexcept { return m_destination; }
        };

        // Load from memory
        class load_memory : public instruction {
        private:
            virtual_register m_destination;
            operand m_source_address;
            size_t m_offset;
        public:
            load_memory(virtual_register destination, operand source_address, size_t offset) 
                : m_destination(destination), m_source_address(source_address), m_offset(offset) {}
        
            virtual_register destination() const noexcept { return m_destination; }
            operand source_address() const noexcept { return m_source_address; }
            size_t offset() const noexcept { return m_offset; }
        };


        // Store to memory
        class store_memory : public instruction {
        private:
            operand m_source_address;
            operand m_value;
            size_t m_offset;
        public:
            store_memory(operand source_address, operand value, size_t offset) 
                : m_source_address(source_address), m_value(value), m_offset(offset) {}
        
            operand source_address() const noexcept { return m_source_address; }
            operand value() const noexcept { return m_value; }
            size_t offset() const noexcept { return m_offset; }
        };

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
            operand m_size;
            size_t m_alignment;

        public:
            valloca_instruction(virtual_register destination, operand size, size_t alignment)
                : m_destination(destination), m_size(size), m_alignment(alignment) {}

            virtual_register destination() const noexcept { return m_destination; }
            operand size() const noexcept { return m_size; }
            size_t alignment() const noexcept { return m_alignment; }
        };

        class memfill : public instruction {
        private:
            operand m_destination;
            operand m_fill_value;
            operand m_fill_count;

        public:
            memfill(operand destination, operand fill_value, operand fill_count)
                : m_destination(destination), m_fill_value(fill_value), m_fill_count(fill_count) {}
            
            operand destination() const noexcept { return m_destination; }
            operand fill_value() const noexcept { return m_fill_value; }
            operand fill_count() const noexcept { return m_fill_count; }
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
            operand m_condition;
            size_t m_if_true_block_id;
            size_t m_if_false_block_id;
            bool m_is_loop; 
        public:
            branch_condition(operand condition, size_t if_true_block_id, size_t if_false_block_id, bool is_loop) 
                : m_condition(condition), m_if_true_block_id(if_true_block_id), m_if_false_block_id(if_false_block_id), m_is_loop(is_loop) {}

            operand condition() const noexcept { return m_condition; }
            size_t if_true_block_id() const noexcept { return m_if_true_block_id; }
            size_t if_false_block_id() const noexcept { return m_if_false_block_id; }
            bool is_loop() const noexcept { return m_is_loop; }
        };


        // Function Stuff

        class function_definition {
        private:
            size_t m_entry_block_id;
            std::string m_name;

        public:
            function_definition(size_t entry_block_id, std::string name) : m_entry_block_id(entry_block_id), m_name(name) {}

            size_t entry_block_id() const noexcept { return m_entry_block_id; }
            const std::string& name() const noexcept { return m_name; }
        };

        class function_call : public instruction {
        private:
            std::optional<virtual_register> m_destination;
            std::shared_ptr<function_definition> m_function_definition;
            std::vector<operand> m_arguments;

        public:
            function_call(std::optional<virtual_register> destination, std::shared_ptr<function_definition> function_definition, std::vector<operand>&& arguments) 
                : m_destination(destination), m_function_definition(function_definition), m_arguments(std::move(arguments)) {}

            std::optional<virtual_register> destination() const noexcept { return m_destination; }
            std::shared_ptr<function_definition> function_definition() const noexcept { return m_function_definition; }
            const std::vector<operand>& arguments() const noexcept { return m_arguments; }
        };

        class function_return : public instruction {
        private:
            std::optional<operand> m_value;

        public:
            function_return(std::optional<operand>&& value) 
                : m_value(std::move(value)) {}
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
