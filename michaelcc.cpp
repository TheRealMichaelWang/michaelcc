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
			cout << element->to_string() << endl;
		}
	}
	catch (const michaelcc::compilation_error& error) {
		cout << error.what() << endl;
	}
	
	return 0;
}
