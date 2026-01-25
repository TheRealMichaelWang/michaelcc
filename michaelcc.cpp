// michaelcc.cpp : Defines the entry point for the application.

#include "syntax/preprocessor.hpp"
#include "syntax/ast.hpp"
#include "syntax/parser.hpp"
#include "logic/semantic.hpp"
#include "logic/optimization/constant_folding.hpp"
#include "logic/optimization/dead_code.hpp"
#include "logic/optimization/constant_prop.hpp"
#include "logic/optimization/ir_simplify.hpp"
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

int main()
{
    cout << "Michael C Compiler" << endl;
	ifstream infile("../../tests/arrays.c");
	
	if (!infile.is_open()) {
		cerr << "Failed to open file!" << endl;
		return 1;
	}

	std::stringstream ss;
	ss << infile.rdbuf();

	try {
		michaelcc::preprocessor preprocessor(ss.str(), "../../tests/arrays.c");
		preprocessor.preprocess();
		vector<michaelcc::token> tokens = preprocessor.result();

		michaelcc::parser parser(std::move(tokens));
		std::vector<std::unique_ptr<michaelcc::ast::ast_element>> ast = parser.parse_all();
		
		// Print all top-level elements
		for (const auto& element : ast) {
			cout << michaelcc::ast::to_c_string(*element) << endl;
		}

		michaelcc::platform_info platform_info{
			.pointer_size = 8,
			.short_size = 2,
			.int_size = 4,
			.long_size = 4,
			.long_long_size = 8,
			.float_size = 4,
			.double_size = 8,
			.max_alignment = 16,
		};

		michaelcc::semantic_lowerer lowerer(platform_info);

		lowerer.lower(ast);
		auto translation_unit = lowerer.release_translation_unit();


		auto passes = std::vector<std::unique_ptr<michaelcc::optimization::pass>>();
		passes.emplace_back(std::make_unique<michaelcc::optimization::constant_folding_pass>(platform_info));
		passes.emplace_back(std::make_unique<michaelcc::optimization::constant_prop_pass>(translation_unit, platform_info));
		passes.emplace_back(std::make_unique<michaelcc::optimization::ir_simplify_pass>());
		passes.emplace_back(std::make_unique<michaelcc::optimization::dead_code_pass>());
		
		int passes_run = michaelcc::optimization::transform(translation_unit, passes);

		cout << michaelcc::logic::to_tree_string(translation_unit) << endl;
		cout << "Passes run: " << passes_run << endl;

		/*for (const auto& symbol : translation_unit.global_symbols()) {
			cout << symbol->name() << endl;
			auto function = std::dynamic_pointer_cast<michaelcc::logic::function_definition>(symbol);
			if (function) {
				cout << "Function: " << function->name() << endl;
				cout << "Return type: " << function->return_type().to_string() << endl;
				cout << "Parameters: " << function->parameters().size() << endl;
			}
		}*/
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
