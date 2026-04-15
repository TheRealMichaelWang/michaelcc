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
#include "linear/pass.hpp"
#include "linear/allocators/remove_phi.hpp"
#include "linear/optimization/dead_code.hpp"
#include "linear/optimization/const_prop.hpp"
#include "linear/optimization/copy_prop.hpp"
#include "linear/optimization/phi.hpp"
#include "isa/isa.hpp"
#include "isa/lc2200.hpp"
#include "CLI11.hpp"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

using namespace std;

struct CompilerOptions {
	std::string input_file;
	std::string output_file;
	std::string platform = "x64";
};

std::unordered_map<std::string, std::unique_ptr<michaelcc::isa::isa>> make_platforms() {
	std::unordered_map<std::string, std::unique_ptr<michaelcc::isa::isa>> map;
	map.emplace("lc2200", std::make_unique<michaelcc::isa::lc2200::lc2200_isa>());
	return map;
}

int main(int argc, char* argv[])
{
	std::unordered_map<std::string, std::unique_ptr<michaelcc::isa::isa>> platforms = make_platforms();
	std::vector<std::string> platform_names;
	for (const auto& [name, _] : platforms)
		platform_names.push_back(name);
	
	CLI::App app("The Michael C Compiler, a basic optimizing C compiler.", "michaelcc");
	argv = app.ensure_utf8(argv);

	CompilerOptions options;
	app.add_option("-i, --input", options.input_file, "The input file to compile")
		->check(CLI::ExistingFile)
		->required();
	app.add_option("-o, --output", options.output_file, "The output file to compile to")
		->required();
	app.add_option("-p, --platform", options.platform, "The platform to compile for")
		->check(CLI::IsMember(platform_names))
		->required();

	CLI11_PARSE(app, argc, argv);

	ifstream infile = std::ifstream(options.input_file);
	
	if (!infile.is_open()) {
		cerr << "Failed to open file!" << endl;
		return 1;
	}

	std::stringstream ss;
	ss << infile.rdbuf();

	try {
		// preprocess the input file
		michaelcc::preprocessor preprocessor(ss.str(), options.input_file);
		preprocessor.preprocess();
		vector<michaelcc::token> tokens = preprocessor.result();

		// parse the tokens into an AST
		michaelcc::parser parser(std::move(tokens));
		std::vector<std::unique_ptr<michaelcc::ast::ast_element>> ast = parser.parse_all();

		michaelcc::isa::isa& platform = *platforms.at(options.platform);
		// lower AST to logical IR
		michaelcc::semantic_lowerer lowerer(platform.get_platform_info());

		lowerer.lower(ast);
		auto logic_translation_unit = lowerer.release_translation_unit();

		auto passes = std::vector<std::unique_ptr<michaelcc::logic::optimization::pass>>();
		passes.emplace_back(michaelcc::logic::optimization::make_constant_folding_pass(platform.get_platform_info()));
		passes.emplace_back(std::make_unique<michaelcc::logic::optimization::ir_simplify_pass>(platform.get_platform_info()));
		passes.emplace_back(std::make_unique<michaelcc::logic::optimization::dead_code_pass>());
		passes.emplace_back(std::make_unique<michaelcc::logic::optimization::inline_functions_pass>());
		passes.emplace_back(std::make_unique<michaelcc::logic::optimization::pointer_propagation_pass>());
		passes.emplace_back(std::make_unique<michaelcc::logic::optimization::const_propagation_pass>(platform.get_platform_info()));
		michaelcc::logic::optimization::transform(logic_translation_unit, passes);
		
		// lower logical IR to linear SSA IR
		michaelcc::logic_lowerer linear_lowerer(platform);
		linear_lowerer.lower(logic_translation_unit);
		auto linear_translation_unit = linear_lowerer.release_translation_unit();

		// optimize the linear IR
		auto linear_passes = std::vector<std::unique_ptr<michaelcc::linear::pass>>();
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::dead_instruction_pass>());
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::dead_block_pass>());
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::const_prop_pass>());
		linear_passes.emplace_back(std::make_unique<michaelcc::linear::optimization::copy_prop_pass>());
		
		michaelcc::linear::transform(linear_translation_unit, linear_passes);


		// allocate stack frame (remove alloca)
		michaelcc::linear::allocators::frame_allocator frame_allocator(linear_translation_unit);
		frame_allocator.allocate();

		michaelcc::linear::transform(linear_translation_unit, linear_passes);

		// remove phi nodes (no optimization passes can be run after this)
		michaelcc::linear::allocators::remove_phi_nodes(linear_translation_unit);

		// register allocation (one pass)
		michaelcc::linear::optimization::postphi::register_allocation(linear_translation_unit, frame_allocator);

		// assemble the linear IR to assembly
		auto file_out_stream = std::ofstream(options.output_file);
		auto assembler = platform.create_assembler(file_out_stream);
		assembler->assemble(linear_translation_unit, frame_allocator);
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
