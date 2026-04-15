cmake_minimum_required(VERSION 3.16)
project(michaelcc LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for IDE/editor support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include directories
include_directories(include)

# Source files
add_executable(michaelcc
    michaelcc.cpp
    print.cpp
    errors.cpp
    syntax/scanner.cpp
    syntax/preprocessor.cpp
    syntax/parser.cpp
    logic/semantic.cpp
    logic/linker.cpp
    logic/type_info.cpp
    logic/optimization.cpp
    logic/constant_folding.cpp
    logic/dead_code.cpp
    logic/ir_simplify.cpp
    linear/flattener.cpp
    linear/static.cpp
    linear/dominators.cpp
    linear/pass.cpp
    linear/dead_code.cpp
    linear/const_prop.cpp
    linear/copy_prop.cpp
    linear/gvn.cpp
    linear/frame_allocator.cpp
    linear/register_allocator.cpp
    linear/register_spiller.cpp
    linear/phi.cpp
    assembly/assembler.cpp
    isa/lc2200_platform.cpp
    isa/lc2200_assembler.cpp
)
