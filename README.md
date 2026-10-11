# CCompiler

A modular, multi-target C compiler written in C++17 with built-in preprocessing, intermediate representation (IR), internal assembly, and an embedded Windows PE linker.

## Quick Start

### Building the Project

Requirements: CMake 3.10+ and a C++17 compiler (GCC/MinGW, Clang, or MSVC).

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Running Tests

```bash
ctest --output-on-failure
```

### Basic Usage

```bash
# Compile and link an executable directly (Windows x86-64)
./build/c_compiler main.c -o app.exe

# Compile multiple files with cross-module linking
./build/c_compiler main.c utils.c math.c -o app.exe

# Output the Intermediate Representation (IR)
./build/c_compiler --emit-ir main.c -o main.ir

# Compile and link AVR firmware directly to Intel HEX (.hex) for Arduino Uno
./build/c_compiler -t avr -I samples/avr samples/avr/blink.c -o blink.hex

# Generate assembly for ARM64 or AVR
./build/c_compiler -t arm64 -S main.c -o main.s
./build/c_compiler -t avr -S main.c -o main.s
```

## Architecture Samples

See the **[`samples/`](samples/)** directory for complete, ready-to-build examples for every architecture:
- **`samples/avr/`**: Arduino Uno bare-metal Pin 13 LED blink program compiling to Intel HEX (`blink.hex`).
- **`samples/x86_64/`**: Native Windows console and Win32 GUI message box binaries.
- **`samples/arm64/`**: AArch64 / Raspberry Pi calling convention and assembly samples.
- **`samples/c/`**: C AST de-sugaring and transpilation demo.

## Documentation

- **[COMPILER.md](COMPILER.md)**: Full technical documentation covering:
  - System architecture and design choices
  - Component implementation (`src/frontend/`, `src/ir/`, `src/backend/`, `src/driver/`)
  - Command-line interface reference and stop phases
  - Target architecture specifications (`x86_64`, `arm64`, `avr`, `c`)
  - Embedded AVR and x86-64 assembler/linker toolchains
  - Architectural roadmap and TODOs
- **[GRAMMAR.md](GRAMMAR.md)**: Formal EBNF grammar specification for all accepted C syntax.
- **[MULTITARGET_REVIEW.md](MULTITARGET_REVIEW.md)**: Multi-target architectural review and design rationale.
