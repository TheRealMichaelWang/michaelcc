#ifndef MICHAELCC_CFG_IR_HPP
#define MICHAELCC_CFG_IR_HPP

#include "logic/ir.hpp"
#include "logic/type_info.hpp"
#include "utils.hpp"
#include "registers.hpp"
#include "static.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace michaelcc {
	namespace linear {
        struct var_info {
            linear::virtual_register vreg;
            size_t block_id;

            bool operator==(const var_info& info) const { return info.vreg == vreg; }
        };

        class instruction {
        public:
            virtual std::optional<linear::virtual_register> destination_register() const noexcept = 0;
            virtual std::vector<linear::virtual_register> operand_registers() const noexcept = 0;
            virtual bool has_side_effects() const noexcept { return false; }

            virtual ~instruction() = default;
        };

        class a_instruction;
        class a2_instruction;
        class u_instruction;
        class copy_instruction;
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
        class load_parameter;
        class phi_instruction;
        class load_effective_address;

        using instruction_transformer = generic_dispatcher<std::unique_ptr<instruction>, const instruction,
            const a_instruction,
            const a2_instruction,
            const u_instruction,
            const copy_instruction,
            const init_register,
            const load_memory,
            const store_memory,
            const alloca_instruction,
            const valloca_instruction,
            const load_parameter,
            const branch,
            const branch_condition,
            const function_call,
            const function_return,
            const phi_instruction,
            const load_effective_address
        >;

        using const_visitor = generic_dispatcher<void, const instruction,
            const a_instruction,
            const a2_instruction,
            const u_instruction,
            const copy_instruction,
            const init_register,
            const load_memory,
            const store_memory,
            const alloca_instruction,
            const valloca_instruction,
            const load_parameter,
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return { m_operand_a, m_operand_b }; }
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return { m_operand_a }; }
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return { m_operand }; }
        };


        // Copy a register and initialize a new register with the value
        class copy_instruction : public instruction {
        private:
            virtual_register m_destination;
            virtual_register m_source;
        public:
            copy_instruction(virtual_register destination, virtual_register source) : m_destination(destination), m_source(source) {}
            
            virtual_register destination() const noexcept { return m_destination; }
            virtual_register source() const noexcept { return m_source; }

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return { m_source }; }
        };
        
        // Initialize a register with a literal value
        class init_register : public instruction {
        private:
            virtual_register m_destination;
            register_word m_value;
        public:
            init_register(virtual_register destination, register_word value) : m_destination(destination), m_value(value) {}
            virtual_register destination() const noexcept { return m_destination; }
            register_word value() const noexcept { return m_value; }

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return {}; }
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return { m_source_address }; }
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return std::nullopt; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return { m_source_address, m_value }; }

            bool has_side_effects() const noexcept override { return true; }
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return {}; }
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return { m_size }; }
        };

        // Branch "B" instructions
        class basic_block {
        private:
            std::vector<std::unique_ptr<instruction>> m_instructions;
            std::vector<size_t> m_successor_block_ids;

            // computed and set after all blocks are created and analyzed
            std::optional<size_t> m_immediate_dominator_block_id;
            std::vector<size_t> m_predecessor_block_ids;
            std::vector<size_t> m_immediately_dominated_block_ids;

            size_t m_id;

        public:
            basic_block(size_t id, std::vector<std::unique_ptr<instruction>>&& instructions, std::vector<size_t>&& successor_block_ids) 
                : m_id(id), m_instructions(std::move(instructions)), m_successor_block_ids(std::move(successor_block_ids)) {}

            size_t id() const noexcept { return m_id; }

            const std::vector<std::unique_ptr<instruction>>& instructions() const noexcept { return m_instructions; }
            const std::vector<size_t>& successor_block_ids() const noexcept { return m_successor_block_ids; }
            const std::vector<size_t>& predecessor_block_ids() const noexcept { return m_predecessor_block_ids; }
            const std::vector<size_t>& immediately_dominated_block_ids() const noexcept { return m_immediately_dominated_block_ids; }

            std::optional<size_t> immediate_dominator_block_id() const noexcept { return m_immediate_dominator_block_id; }

            std::vector<std::unique_ptr<instruction>>&& release_instructions() noexcept { return std::move(m_instructions); }   
            
            void add_predecessor_block_id(size_t block_id) {
                m_predecessor_block_ids.push_back(block_id);
            }

            void set_dominator_info(std::optional<size_t>&& immediate_dominator_block_id, std::vector<size_t>&& ids) {
                m_immediate_dominator_block_id = std::move(immediate_dominator_block_id);
                m_immediately_dominated_block_ids = std::move(ids);
            }

            void replace_instructions(std::vector<std::unique_ptr<instruction>>&& instructions) {
                m_instructions = std::move(instructions);
            }
        };

        class branch : public instruction {
        private:
            size_t m_next_block_id;

        public:
            branch(size_t next_block_id) : m_next_block_id(next_block_id) {}

            size_t next_block_id() const noexcept { return m_next_block_id; }

            std::optional<linear::virtual_register> destination_register() const noexcept override { return std::nullopt; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return {}; }

            bool has_side_effects() const noexcept override { return true; }
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return std::nullopt; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return { m_condition }; }

            bool has_side_effects() const noexcept override { return true; }
        };


        // Function Stuff
        struct function_parameter {
            std::string name;

            type_layout_info layout;
            bool pass_as_alloca;
        };

        class function_definition {
        private:
            std::string m_name;

            size_t m_entry_block_id;
            std::vector<function_parameter> m_parameters;

        public:
            function_definition(std::string name, size_t entry_block_id, std::vector<function_parameter>&& parameters) 
                : m_name(name), m_entry_block_id(entry_block_id), m_parameters(std::move(parameters)) {}
            
            size_t entry_block_id() const noexcept { return m_entry_block_id; }
            const std::vector<function_parameter>& parameters() const noexcept { return m_parameters; }
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return m_arguments; }

            bool has_side_effects() const noexcept override { return true; }
        };

        class function_return : public instruction {
        public:
            function_return() { }

            std::optional<linear::virtual_register> destination_register() const noexcept override { return std::nullopt; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return {}; }

            bool has_side_effects() const noexcept override { return true; }
        };


        // load parameter from the stack; stores the value or stores the address of the stack location into the destination register
        class load_parameter : public instruction {
        private:
            virtual_register m_destination;
            function_parameter m_parameter;
            
        public:
            load_parameter(virtual_register destination, function_parameter parameter) : 
                m_destination(destination), m_parameter(parameter) {}

            virtual_register destination() const noexcept { return m_destination; }
            const function_parameter& parameter() const noexcept { return m_parameter; }

            bool is_address() const noexcept { return m_parameter.pass_as_alloca; }

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return {}; }
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

            void augment_value(const var_info& value) {
                if (std::find(m_values.begin(), m_values.end(), value) == m_values.end() && m_destination != value.vreg) {
                    m_values.push_back(value);
                }
            }

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { 
                std::vector<linear::virtual_register> operands;
                operands.reserve(m_values.size());
                for (const auto& value : m_values) {
                    operands.push_back(value.vreg);
                }
                return operands;
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

            std::optional<linear::virtual_register> destination_register() const noexcept override { return m_destination; }
            std::vector<linear::virtual_register> operand_registers() const noexcept override { return {}; }
        };

        struct translation_unit {
            std::vector<std::unique_ptr<function_definition>> function_definitions;
            std::unordered_map<size_t, linear::basic_block> blocks;

            register_allocator register_allocator;
            static_storage::static_sections static_sections;
            const platform_info& platform_info;
        };

        std::string print_linear_ir(const translation_unit& unit);
	}
}

#endif
