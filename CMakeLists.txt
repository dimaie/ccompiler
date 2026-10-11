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

include_directories(
    src/frontend
    src/ir
    src/backend
    src/driver
)

set(FRONTEND_SOURCES
    src/frontend/lexer.cpp
    src/frontend/parser_core.cpp
    src/frontend/parser_decl.cpp
    src/frontend/parser_expr.cpp
    src/frontend/parser_stmt.cpp
    src/frontend/preprocessor.cpp
)

set(IR_SOURCES
    src/ir/ir.cpp
    src/ir/ir_builder.cpp
)

set(BACKEND_SOURCES
    src/backend/codegen.cpp
    src/backend/codegen_x86_64.cpp
    src/backend/codegen_arm64.cpp
    src/backend/codegen_avr.cpp
    src/backend/codegen_c.cpp
    src/backend/internal_assembler.cpp
    src/backend/internal_pe_linker.cpp
    src/backend/assembler_linker.cpp
)

set(DRIVER_SOURCES
    src/driver/compiler_config.cpp
)

# Compiler executable
add_executable(c_compiler
    ${FRONTEND_SOURCES}
    ${IR_SOURCES}
    ${BACKEND_SOURCES}
    ${DRIVER_SOURCES}
    src/driver/main.cpp
)

# Parser test executable
add_executable(test_parser
    ${FRONTEND_SOURCES}
    tests/test_parser.cpp
)

# Codegen test executable
add_executable(test_codegen
    ${FRONTEND_SOURCES}
    ${IR_SOURCES}
    ${BACKEND_SOURCES}
    ${DRIVER_SOURCES}
    tests/test_codegen.cpp
)

# IR test executable
add_executable(test_ir
    ${FRONTEND_SOURCES}
    ${IR_SOURCES}
    tests/test_ir.cpp
)

enable_testing()
add_test(NAME ParserTests COMMAND test_parser)
add_test(NAME CodegenTests COMMAND test_codegen)
add_test(NAME IRTests COMMAND test_ir)