// michaelcc.cpp : Defines the entry point for the application.

#include "preprocessor.hpp"
#include "ast.hpp"
#include "parser.hpp"
#include <fstream>
#include <iostream>

using namespace std;

int main()
{
    cout << "Michael C Compiler" << endl;
	ifstream infile("../../tests/main.c");
	
	if (!infile.is_open()) {
		cerr << "Failed to open file!" << endl;
		return 1;
	}

	std::stringstream ss;
	ss << infile.rdbuf();

	try {
		michaelcc::preprocessor preprocessor(ss.str(), "../../tests/main.c");
		preprocessor.preprocess();
		vector<michaelcc::token> tokens = preprocessor.result();

		michaelcc::parser parser(std::move(tokens));
		std::vector<std::unique_ptr<michaelcc::ast::ast_element>> ast = parser.parse_all();
		
		// Print all top-level elements
		for (const auto& element : ast) {
			cout << michaelcc::ast::to_c_string(element.get()) << endl;
		}
	}
	catch (const michaelcc::compilation_error& error) {
		cerr << error.what() << endl;
	}
	
	return 0;
}
