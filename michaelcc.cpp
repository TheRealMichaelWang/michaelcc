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
	ifstream infile("tests/data_structures.c");
	std::stringstream ss;
	ss << infile.rdbuf();

	michaelcc::preprocessor preprocessor(ss.str(), "tests/data_structures.c");
	preprocessor.preprocess();
	vector<michaelcc::token> tokens = preprocessor.result();

	michaelcc::parser parser(std::move(tokens));
	try {
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
