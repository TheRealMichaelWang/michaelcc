// michaelcc.cpp : Defines the entry point for the application.

#include "preprocessor.hpp"
#include "ast.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include <fstream>
#include <iostream>

using namespace std;

int main()
{
    cout << "Michael C Compiler" << endl;
	ifstream infile("../../tests/arithmetic.c");
	
	if (!infile.is_open()) {
		cerr << "Failed to open file!" << endl;
		return 1;
	}

	std::stringstream ss;
	ss << infile.rdbuf();

	try {
		michaelcc::preprocessor preprocessor(ss.str(), "../../tests/arithmetic.c");
		preprocessor.preprocess();
		vector<michaelcc::token> tokens = preprocessor.result();

		michaelcc::parser parser(std::move(tokens));
		std::vector<std::unique_ptr<michaelcc::ast::ast_element>> ast = parser.parse_all();
		
		// Print all top-level elements
		for (const auto& element : ast) {
			cout << michaelcc::ast::to_c_string(*element) << endl;
		}

		michaelcc::compiler compiler(michaelcc::compiler::platform_info{
			.m_pointer_size = 8,
			.m_int_size = 4,
			.m_short_size = 2,
			.m_long_size = 4,
			.m_long_long_size = 8,
			.m_float_size = 4,
			.m_double_size = 8,
		});

		compiler.compile(ast);
		auto translation_unit = compiler.release_translation_unit();

		for (const auto& symbol : translation_unit.global_symbols()) {
			cout << symbol->name() << endl;
			auto function = std::dynamic_pointer_cast<michaelcc::logical_ir::function_definition>(symbol);
			if (function) {
				cout << "Function: " << function->name() << endl;
				cout << "Return type: " << function->return_type().to_string() << endl;
				cout << "Parameters: " << function->parameters().size() << endl;
			}
		}
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
