#include "preprocessor.hpp"
#include "parser.hpp"
#include "semantic_analyzer.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
	std::cout << "michaelcc - C Compiler\n";

	std::filesystem::path input = "../../tests/main.c";
	if (argc > 1) input = argv[1];

	std::ifstream file(input);
	if (!file) {
		std::cerr << "error: cannot open " << input << "\n";
		return 1;
	}

	std::ostringstream ss;
	ss << file.rdbuf();

	try {
		michaelcc::preprocessor pp(ss.str(), input);
		pp.run();
		auto tokens = pp.take_result();

		michaelcc::parser parser(std::move(tokens));
		auto ast = parser.parse();

		michaelcc::analyzer analyzer;
		auto unit = analyzer.analyze(ast);

		std::cout << "\nFunctions: " << unit.funcs().size() << "\n";
		for (auto& f : unit.funcs()) {
			std::cout << "  " << f->name << ": " << f->ftype->str() << "\n";
		}

		std::cout << "Globals: " << unit.globals().size() << "\n";
		for (auto& v : unit.globals()) {
			std::cout << "  " << v->name << ": " << v->vtype->str() << "\n";
		}

		std::cout << "Strings: " << unit.strings().size() << "\n";
		for (size_t i = 0; i < unit.strings().size(); ++i) {
			std::cout << "  [" << i << "] \"" << unit.strings()[i] << "\"\n";
		}

	} catch (const michaelcc::compile_error& e) {
		std::cerr << e.what() << "\n";
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "error: " << e.what() << "\n";
		return 1;
	}

	std::cout << "\nDone.\n";
	return 0;
}
