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
    scanner.cpp
    errors.cpp
    preprocessor.cpp
    print.cpp
    parser.cpp
    semantic.cpp
    linker.cpp
    flattener.cpp
    type_info.cpp
    linear/registers.cpp
    linear/static.cpp
    linear/dominators.cpp
    linear/optimization.cpp
    linear/dead_code.cpp
    linear/const_prop.cpp
    linear/copy_prop.cpp
    linear/gvn.cpp
    logic/optimization.cpp
    logic/constant_folding.cpp
    logic/dead_code.cpp
    logic/ir_simplify.cpp
)
