// michaelcc.cpp : Defines the entry point for the application.

#include "syntax/preprocessor.hpp"
#include "syntax/ast.hpp"
#include "syntax/parser.hpp"
#include "logic/semantic.hpp"
#include "logic/optimization/constant_folding.hpp"
#include "logic/optimization/dead_code.hpp"
#include "logic/optimization/ir_simplify.hpp"
#include "logic/optimization/inline_functions.hpp"
#include "logic/optimization/pointer_propagation.hpp"
#include "logic/optimization/const_propagation.hpp"	
#include "linear/flatten.hpp"
#include "linear/optimization.hpp"
#include "linear/optimization/dead_code.hpp"
#include "linear/optimization/const_prop.hpp"
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

int main(int argc, char* argv[])
{
	std::vector<std::string_view> args(argv, argv + argc);

    cout << "Michael C Compiler" << endl;
	auto path = args.size() > 1 ? args.at(1) : "../../tests/constant_folding.c";
	ifstream infile = std::ifstream(std::string(path));
	
	if (!infile.is_open()) {
		cerr << "Failed to open file!" << endl;
		return 1;
	}

	std::stringstream ss;
	ss << infile.rdbuf();

	try {
		michaelcc::preprocessor preprocessor(ss.str(), path);
		preprocessor.preprocess();
		vector<michaelcc::token> tokens = preprocessor.result();

		michaelcc::parser parser(std::move(tokens));
		std::vector<std::unique_ptr<michaelcc::ast::ast_element>> ast = parser.parse_all();
		
		// Print all top-level elements
		for (const auto& element : ast) {
			cout << michaelcc::ast::to_c_string(*element) << endl;
		}

		michaelcc::platform_info x64_platform_info{
			.pointer_size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64,
			.short_size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16,
			.int_size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32,
			.long_size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32,
			.long_long_size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64,
			.float_size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32,
			.double_size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64,
			.max_alignment = 8,

			.return_register_int_id = 0,
			.return_register_float_id = 0,

			.registers = {
				{
					.id = 0,
					.name = "rax",
					.description = "RAX/Return Value Register",
					.mutually_exclusive_registers = { 1, 2, 3, 4 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 1,
					.name = "eax",
					.description = "EAX",
					.mutually_exclusive_registers = { 0, 2, 3, 4 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32
				},
				{
					.id = 2,
					.name = "ax",
					.description = "AX",
					.mutually_exclusive_registers = { 0, 1, 3, 4 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16
				},
				{
					.id = 3,
					.name = "al",
					.description = "AL/Lower 8 bits of AX",
					.mutually_exclusive_registers = { 0, 1, 2 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE
				},
				{
					.id = 4,
					.name = "ah",
					.description = "AH/Upper 8 bits of AX",
					.mutually_exclusive_registers = { 0, 1, 2 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE
				},
				{
					.id = 5,
					.name = "rbx",
					.description = "RBX",
					.mutually_exclusive_registers = { 6, 7, 8, 9 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 6,
					.name = "ebx",
					.description = "EBX",
					.mutually_exclusive_registers = { 5, 7, 8, 9 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32
				},
				{
					.id = 7,
					.name = "bx",
					.description = "BX",
					.mutually_exclusive_registers = { 5, 6, 8, 9 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16
				},
				{
					.id = 8,
					.name = "bl",
					.description = "BL",
					.mutually_exclusive_registers = { 5, 6, 7 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE
				},
				{
					.id = 9,
					.name = "bh",
					.description = "BH",
					.mutually_exclusive_registers = { 5, 6, 7 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE
				},
				{
					.id = 10,
					.name = "rcx",
					.description = "RCX",
					.mutually_exclusive_registers = { 11, 12, 13, 14 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 11,
					.name = "ecx",
					.description = "ECX",
					.mutually_exclusive_registers = { 10, 12, 13, 14 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32
				},
				{
					.id = 12,
					.name = "cx",
					.description = "CX",
					.mutually_exclusive_registers = { 10, 11, 13, 14 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16
				},
				{
					.id = 13,
					.name = "cl",
					.description = "CL",
					.mutually_exclusive_registers = { 10, 11, 12 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE
				},
				{
					.id = 14,
					.name = "ch",
					.description = "CH",
					.mutually_exclusive_registers = { 10, 11, 12 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE
				},
				{
					.id = 15,
					.name = "rdx",
					.description = "RDX",
					.mutually_exclusive_registers = { 16, 17, 18, 19 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 16,
					.name = "edx",
					.description = "EDX",
					.mutually_exclusive_registers = { 15, 17, 18, 19 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32
				},
				{
					.id = 17,
					.name = "dx",
					.description = "DX",
					.mutually_exclusive_registers = { 15, 16, 18, 19 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16
				},
				{
					.id = 18,
					.name = "dl",
					.description = "DL",
					.mutually_exclusive_registers = { 15, 16, 17 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE
				},
				{
					.id = 19,
					.name = "dh",
					.description = "DH",
					.mutually_exclusive_registers = { 15, 16, 17 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE
				},
				{
					.id = 20,
					.name = "rsi",
					.description = "RSI",
					.mutually_exclusive_registers = { 21, 22, 23 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 21,
					.name = "esi",
					.description = "ESI",
					.mutually_exclusive_registers = { 20, 22, 23 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32
				},
				{
					.id = 22,
					.name = "si",
					.description = "SI",
					.mutually_exclusive_registers = { 20, 21, 23 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16
				},
				{
					.id = 24,
					.name = "rdi",
					.description = "RDI",
					.mutually_exclusive_registers = { 25, 26, 27 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 25,
					.name = "edi",
					.description = "EDI",
					.mutually_exclusive_registers = { 24, 26, 27 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32
				},
				{
					.id = 26,
					.name = "di",
					.description = "DI",
					.mutually_exclusive_registers = { 24, 25, 27 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16
				},
				{
					.id = 28,
					.name = "rbp",
					.description = "RBP",
					.mutually_exclusive_registers = { 29, 30, 31 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 29,
					.name = "ebp",
					.description = "EBP",
					.mutually_exclusive_registers = { 28, 30, 31 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32
				},
				{
					.id = 30,
					.name = "bp",
					.description = "BP",
					.mutually_exclusive_registers = { 28, 29, 31 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16
				},
				{
					.id = 32,
					.name = "rsp",
					.description = "RSP",
					.mutually_exclusive_registers = { 33, 34, 35 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 33,
					.name = "esp",
					.description = "ESP",
					.mutually_exclusive_registers = { 32, 34, 35 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32
				},
				{
					.id = 34,
					.name = "sp",
					.description = "SP",
					.mutually_exclusive_registers = { 32, 33, 35 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16
				},
				{
					.id = 36,
					.name = "r8",
					.description = "R8",
					.mutually_exclusive_registers = { 37, 38, 39 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 40,
					.name = "r9",
					.description = "R9",
					.mutually_exclusive_registers = { 41, 42, 43 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 44,
					.name = "r10",
					.description = "R10",
					.mutually_exclusive_registers = { 45, 46, 47 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 48,
					.name = "r11",
					.description = "R11",
					.mutually_exclusive_registers = { 49, 50, 51 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 52,
					.name = "r12",
					.description = "R12",
					.mutually_exclusive_registers = { 53, 54, 55 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 56,
					.name = "r13",
					.description = "R13",
					.mutually_exclusive_registers = { 57, 58, 59 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 60,
					.name = "r14",
					.description = "R14",
					.mutually_exclusive_registers = { 61, 62, 63 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				},
				{
					.id = 64,
					.name = "r15",
					.description = "R15",
					.mutually_exclusive_registers = { 65, 66, 67 },
					.size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64
				}
			}
		};

		michaelcc::semantic_lowerer lowerer(x64_platform_info);

		lowerer.lower(ast);
		auto logic_translation_unit = lowerer.release_translation_unit();

		auto passes = std::vector<std::unique_ptr<michaelcc::logic::optimization::pass>>();
		//passes.emplace_back(michaelcc::logic::optimization::make_constant_folding_pass(x64_platform_info));
		passes.emplace_back(std::make_unique<michaelcc::logic::optimization::ir_simplify_pass>(x64_platform_info));
		//passes.emplace_back(std::make_unique<michaelcc::logic::optimization::dead_code_pass>());
		//passes.emplace_back(std::make_unique<michaelcc::logic::optimization::inline_functions_pass>());
		//passes.emplace_back(std::make_unique<michaelcc::logic::optimization::pointer_propagation_pass>());
		//passes.emplace_back(std::make_unique<michaelcc::logic::optimization::const_propagation_pass>(x64_platform_info));
		michaelcc::logic::optimization::transform(logic_translation_unit, passes);
		
		cout << michaelcc::logic::to_tree_string(logic_translation_unit) << endl;

		michaelcc::logic_lowerer linear_lowerer(x64_platform_info);
		linear_lowerer.lower(logic_translation_unit);
		auto linear_translation_unit = linear_lowerer.release_translation_unit();

		auto linear_passes = std::vector<std::unique_ptr<michaelcc::linear::optimization::pass>>();
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::dead_instruction_pass>());
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::dead_block_pass>());
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::const_prop_pass>());
		michaelcc::linear::optimization::transform(linear_translation_unit, linear_passes);

		cout << michaelcc::linear::print_linear_ir(linear_translation_unit) << endl;
	}
	catch (const michaelcc::compilation_error& error) {
		cerr << "Compilation error: " << error.what() << endl;
		return 2;
	}
	catch (const std::exception& e) {
		cerr << "Exception: " << e.what() << endl;
		return 3;
	}
	catch (...) {
		cerr << "Unknown exception caught!" << endl;
		return 4;
	}
	
	return 0;
}
