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
#include "linear/optimization/copy_prop.hpp"
#include "isa/x64.hpp"
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

int main(int argc, char* argv[])
{
	std::vector<std::string_view> args(argv, argv + argc);

    cout << "Michael C Compiler" << endl;
	auto path = args.size() > 1 ? args.at(1) : "../../tests/loops.c";
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

		michaelcc::semantic_lowerer lowerer(michaelcc::isa::x64::platform_info);

		lowerer.lower(ast);
		auto logic_translation_unit = lowerer.release_translation_unit();

		auto passes = std::vector<std::unique_ptr<michaelcc::logic::optimization::pass>>();
		//passes.emplace_back(michaelcc::logic::optimization::make_constant_folding_pass(x64_platform_info));
		passes.emplace_back(std::make_unique<michaelcc::logic::optimization::ir_simplify_pass>(michaelcc::isa::x64::platform_info));
		//passes.emplace_back(std::make_unique<michaelcc::logic::optimization::dead_code_pass>());
		//passes.emplace_back(std::make_unique<michaelcc::logic::optimization::inline_functions_pass>());
		//passes.emplace_back(std::make_unique<michaelcc::logic::optimization::pointer_propagation_pass>());
		//passes.emplace_back(std::make_unique<michaelcc::logic::optimization::const_propagation_pass>(x64_platform_info));
		michaelcc::logic::optimization::transform(logic_translation_unit, passes);
		
		cout << michaelcc::logic::to_tree_string(logic_translation_unit) << endl;

		michaelcc::logic_lowerer linear_lowerer(michaelcc::isa::x64::platform_info);
		linear_lowerer.lower(logic_translation_unit);
		auto linear_translation_unit = linear_lowerer.release_translation_unit();

		auto linear_passes = std::vector<std::unique_ptr<michaelcc::linear::optimization::pass>>();
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::dead_instruction_pass>());
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::dead_block_pass>());
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::const_prop_pass>());
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::copy_prop_pass>());
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
