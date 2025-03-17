// michaelcc.cpp : Defines the entry point for the application.
//
#include "preprocessor.hpp"
#include "ast.hpp"
#include <fstream>

using namespace std;

int main()
{
	ifstream infile("tests/fib.c");
	std::stringstream ss;
	ss << infile.rdbuf();

	michaelcc::preprocessor preprocessor(ss.str(), "tests/fib.c");
	preprocessor.preprocess();
	const auto& tokens = preprocessor.result();
	return 0;
}
