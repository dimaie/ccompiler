cmake_minimum_required(VERSION 3.10)
project(c_compiler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug symbols and warnings
if (MSVC)
    add_compile_options(/W4 /Zi)
else()
    add_compile_options(-Wall -Wextra -g)
    add_link_options(-static -static-libgcc -static-libstdc++)
endif()

set(PARSER_SOURCES
    lexer.cpp
    parser_core.cpp
    parser_decl.cpp
    parser_expr.cpp
    parser_stmt.cpp
    preprocessor.cpp
)

set(CODEGEN_SOURCES
    codegen.cpp
    codegen_x86_64.cpp
    codegen_arm64.cpp
    codegen_avr.cpp
    codegen_c.cpp
    internal_assembler.cpp
    internal_pe_linker.cpp
    assembler_linker.cpp
    compiler_config.cpp
)

# Compiler executable
add_executable(c_compiler
    ${PARSER_SOURCES}
    ${CODEGEN_SOURCES}
    main.cpp
)

# Parser test executable
add_executable(test_parser
    ${PARSER_SOURCES}
    test_parser.cpp
)

# Codegen test executable
add_executable(test_codegen
    ${PARSER_SOURCES}
    ${CODEGEN_SOURCES}
    test_codegen.cpp
)

enable_testing()
add_test(NAME ParserTests COMMAND test_parser)
add_test(NAME CodegenTests COMMAND test_codegen)