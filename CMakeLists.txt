cmake_minimum_required(VERSION 3.16)
project(michaelcc LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for IDE/editor support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Source files
add_executable(michaelcc
    michaelcc.cpp
    scanner.cpp
    errors.cpp
    preprocessor.cpp
    print.cpp
    parser.cpp
)
