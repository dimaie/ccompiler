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

# Generate ZX Spectrum 48K Z80 assembly (assemble externally with sjasmplus)
./build/c_compiler -t z80-spectrum -S samples/z80/attributes.c -o attributes.asm
sjasmplus --raw=attributes.bin attributes.asm
```

## Architecture Samples

See the **[`samples/`](samples/)** directory for complete, ready-to-build examples for every architecture:
- **`samples/avr/`**: Arduino Uno bare-metal Pin 13 LED blink program compiling to Intel HEX (`blink.hex`).
- **`samples/x86_64/`**: Native Windows console and Win32 GUI message box binaries.
- **`samples/arm64/`**: AArch64 / Raspberry Pi calling convention and assembly samples.
- **`samples/c/`**: C AST de-sugaring and transpilation demo.
- **`samples/z80/`**: ZX Spectrum 48K attribute-memory stripes; see [Spectrum usage and limitations](#zx-spectrum-z80).

## ZX Spectrum (Z80)

Use `-t z80-spectrum` (aliases: `z80`, `spectrum`, `zx-spectrum`, `zxspectrum`).
Output is **sjasmplus assembly only**; no built-in Z80 assembler, linker, TAP
packager, or C standard library is provided. Use a single translation unit with
`int main(void)`. `-S` writes assembly to `-o` or stdout; a normal build stops at
assembly using the existing assembly-only target workflow.

The generated program starts at **32768**. After assembling the example above,
load the raw binary into a Spectrum emulator at 32768, or package it externally
as a tape CODE block. From BASIC, reserve memory before loading:
`CLEAR 32767`, then `LOAD "" CODE` for a tape CODE block, then
`RANDOMIZE USR 32768`. The entry stub returns `main`'s result in BC.

- Little-endian, signed 8-bit `char`, 16-bit `short`/`int` and pointers.
- Re-entrant stack locals and calls; arguments pushed right-to-left in two-byte
  slots, caller cleanup, HL return value, IX frame pointer.
- Integer arithmetic, bitwise operations, signed comparisons, short-circuit
  logic, loops, switch, goto, ternary expressions, pointers, one-dimensional
  arrays, strings, globals, and inline Z80 assembly.
- Global initializers and case values must be integer literals (optionally
  unary `+`/`-`); array initializers use brace lists, with omitted elements zeroed.
- Unsupported constructs fail explicitly: `long`/`long long`, structs/unions,
  function pointers, variadic/non-cdecl calls, static/extern locals,
  pointer subtraction between two pointers, and top-level executable statements.
  The shared frontend's existing syntax limitations also apply.

Inline assembly must preserve IX, SP, IY, and the alternate register set.
Generated code leaves IY and the alternate registers untouched for the Spectrum
ROM. Keep the assembled image and runtime stack within available RAM without
overlap; there is no automatic memory placement or stack-overflow checking.

## Documentation

- **[COMPILER.md](COMPILER.md)**: Full technical documentation covering:
  - System architecture and design choices
  - Component implementation (`src/frontend/`, `src/ir/`, `src/backend/`, `src/driver/`)
  - Command-line interface reference and stop phases
  - Target architecture specifications (`x86_64`, `arm64`, `avr`, `z80`, `c`)
  - Embedded AVR and x86-64 assembler/linker toolchains
  - Architectural roadmap and TODOs
- **[GRAMMAR.md](GRAMMAR.md)**: Formal EBNF grammar specification for all accepted C syntax.
- **[MULTITARGET_REVIEW.md](MULTITARGET_REVIEW.md)**: Multi-target architectural review and design rationale.
