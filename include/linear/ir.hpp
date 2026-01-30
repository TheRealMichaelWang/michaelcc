#ifndef MICHAELCC_CFG_IR_HPP
#define MICHAELCC_CFG_IR_HPP

#include "logic/ir.hpp"
#include <cstddef>

namespace michaelcc {
	namespace linear {
        enum size_type {
            BYTE,
            HALF_WORD,
            WORD,
            DOUBLE_WORD
        };

        struct virtual_register { size_t id; size_type size; };
        struct literal { uint64_t value; size_type size; };

        using operand = std::variant<virtual_register, literal>;

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


        // Memory "M" instructions
        enum m_instruction_type {
            LOAD,
            STORE,
            ALLOCATE
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


        // Branch "B" instructions
        class basic_block {
        private:
            std::vector<std::unique_ptr<instruction>> m_instructions;
            size_t m_id;

        public:
            basic_block(size_t id) : m_id(id) {}

            size_t id() const noexcept { return m_id; }
            const std::vector<std::unique_ptr<instruction>>& instructions() const noexcept { return m_instructions; }
            std::vector<std::unique_ptr<instruction>> release_instructions() noexcept { return std::move(m_instructions); }

            void add_instruction(std::unique_ptr<instruction>&& instruction) {
                m_instructions.push_back(std::move(instruction));
            }   
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
        public:
            branch_condition(operand condition, size_t if_true_block_id, size_t if_false_block_id) 
                : m_condition(condition), m_if_true_block_id(if_true_block_id), m_if_false_block_id(if_false_block_id) {}
            operand condition() const noexcept { return m_condition; }
            size_t if_true_block_id() const noexcept { return m_if_true_block_id; }
            size_t if_false_block_id() const noexcept { return m_if_false_block_id; }
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
            std::vector<std::pair<size_t, operand>> m_values;

        public:
            phi_instruction(virtual_register destination, std::vector<std::pair<size_t, operand>>&& values) 
                : m_destination(destination), m_values(std::move(values)) {}

            virtual_register destination() const noexcept { return m_destination; }
            const std::vector<std::pair<size_t, operand>>& values() const noexcept { return m_values; }
        };
	}
}

#endif
