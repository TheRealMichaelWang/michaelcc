// michaelcc.cpp : Defines the entry point for the application.
//
#include "preprocessor.hpp"
#include "ast.hpp"
#include "parser.hpp"
#include <fstream>
#include <iostream>

using namespace std;

int main()
{
	ifstream infile("tests/main.c");
	std::stringstream ss;
	ss << infile.rdbuf();

	try {
		michaelcc::preprocessor preprocessor(ss.str(), "tests/main.c");
		preprocessor.preprocess();
		vector<michaelcc::token> tokens = preprocessor.result();

		michaelcc::parser parser(std::move(tokens));
		auto result = parser.parse_all();
		for (auto& element : result) {
			cout << element->to_string() << endl;
		}
	}
	catch (const michaelcc::compilation_error& error) {
		cout << error.what() << endl;
	}
	
	return 0;
}
