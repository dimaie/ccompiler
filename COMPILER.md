# CCompiler – Comprehensive Architecture, Implementation & Reference Guide

This document is the authoritative technical reference for **CCompiler**, a lightweight, self-hosting-oriented C compiler written in C++17. It details the system architecture, design decisions, implementation breakdown, command-line interface, target-specific backends, and current roadmap/TODOs.

---

## Table of Contents

1. [Architecture & Design Choices](#1-architecture--design-choices)
   - [Pipeline Overview](#pipeline-overview)
   - [Directory & Component Layout](#directory--component-layout)
   - [Architectural Design Decisions](#architectural-design-decisions)
2. [Component Implementation](#2-component-implementation)
   - [Frontend (`src/frontend/`)](#frontend-srcfrontend)
   - [Middle-End & Intermediate Representation (`src/ir/`)](#middle-end--intermediate-representation-srcir)
   - [Backend & Code Generators (`src/backend/`)](#backend--code-generators-srcbackend)
   - [Internal Assembler & PE Linker (`src/backend/`)](#internal-assembler--pe-linker-srcbackend)
   - [Driver & Configuration (`src/driver/`)](#driver--configuration-srcdriver)
3. [User Interface & CLI](#3-user-interface--cli)
   - [Options Reference](#options-reference)
   - [Stop Phases](#stop-phases)
   - [Usage Examples](#usage-examples)
4. [Target Architectures](#4-target-architectures)
   - [x86_64 (Windows x64 / AMD64)](#x86_64-windows-x64--amd64)
   - [Linux ELF64 (x86_64)](#linux-elf64-x86_64)
   - [ARM64 / AArch64](#arm64--aarch64)
   - [AVR (8-bit Microcontroller / Arduino)](#avr-8-bit-microcontroller--arduino)
   - [C Transpiler](#c-transpiler)
5. [Roadmap & TODOs](#5-roadmap--todos)

---

## 1. Architecture & Design Choices

### Pipeline Overview

The compilation pipeline operates as a multi-stage transform with early-exit stopping phases:

```
Source Files (.c, .h)
       │
       ▼
┌──────────────────┐
│   Preprocessor   │ ── (-E) ──> Preprocessed Output
└──────────────────┘
       │
       ▼
┌──────────────────┐
│  Lexer & Parser  │ ── (--ast) ──> Abstract Syntax Tree (AST)
└──────────────────┘
       │
       ▼
┌──────────────────┐
│  IR Lowering &   │ ── (--emit-ir, --ir) ──> Three-Address Code (TAC) IR
│   TargetInfo     │
└──────────────────┘
       │
       ▼
┌──────────────────┐
│  Code Generator  │ ── (-S) ──> Assembly Output (.s)
└──────────────────┘
       │
       ▼
┌──────────────────┐
│ Internal Assembler│ ── (-c) ──> In-Memory Assembled Modules (COFF sections)
└──────────────────┘
       │
       ▼
┌──────────────────┐
│ Embedded PE      │ ── (FullBuild) ──> Executable (.exe) or DLL (.dll)
│ Linker           │
└──────────────────┘
```

### Directory & Component Layout

The codebase is organized into cleanly decoupled directories:

| Directory | Responsibilities | Key Files |
| :--- | :--- | :--- |
| `src/frontend/` | Lexical analysis, preprocessing, recursive-descent parsing, and AST nodes. | `lexer.*`, `parser_*.cpp`, `preprocessor.*`, `ast.hpp` |
| `src/ir/` | Target-independent 3-Address Code (TAC) IR, lowering engine, and target descriptions. | `ir.*`, `ir_builder.*`, `target_info.hpp` |
| `src/backend/` | Target-specific assembly codegen, assemblers, and linkers organized into per-architecture directories. | `codegen.*`, `assembler_linker.*`, `x86_64/`, `avr/`, `arm64/`, `c/` |
| `src/driver/` | Configuration state, POSIX/GNU CLI argument parser, and compilation driver. | `compiler_config.*`, `main.cpp` |
| `tests/` | Unit and integration test suites for parser, code generators, linker, and IR. | `test_parser.cpp`, `test_codegen.cpp`, `test_ir.cpp` |

### Architectural Design Decisions

1. **"Incomplete rather than incorrect" Philosophy**:
   - The compiler prioritizes explicit errors and strict feature boundaries (`UnsupportedFeatureException`) over silent incorrect code generation or bogus fallbacks.
2. **Intermediate Representation (Middle-End Stage)**:
   - Eliminates direct $M \times N$ AST-to-text duplication across backends.
   - Lowers high-level control structures (`if`, `for`, `while`, `do-while`, `switch`), short-circuit operators (`&&`, `||`), and expressions into a standardized linear Three-Address Code (TAC) representation with explicit control-flow labels.
3. **Machine Description Layer (`TargetInfo`) & Storage Modes**:
   - Replaces magic numbers scattered across backends. Encapsulates integer widths, pointer sizes, alignment, and endianness per architecture (`AVR` uses 16-bit pointers and integers; `x86_64` and `ARM64` use 64-bit pointers and 32-bit integers).
   - Encapsulates local variable storage configuration (`LocalVarStorage::Stack` vs `LocalVarStorage::Memory`), allowing the middle-end IR to lower variables explicitly into stack frame registers or static SRAM memory locations.
4. **Multi-Compilation Unit Architecture**:
   - Compiles multiple source files within a single execution.
   - Merges `.text` and `.data` sections with 16-byte alignment.
   - Resolves cross-module relocations and symbols in memory.
5. **Static Function Linkage Scoping**:
   - Functions marked `static` emit `.local` assembly directives and are tracked in `AssembledModule::local_symbols`.
   - The embedded linker decorates local symbols with a per-module identifier (`symbol@mod<id>`), allowing different translation units to define static helpers with identical names without collisions.
6. **Automatic Target File Type Identification**:
   - Evaluates all translation units for the presence of a `main` entry point.
   - If `main` is present, defaults to producing an executable (`TargetFileType::Executable`).
   - If `main` is absent, or if `-c` is passed, identifies the target as an object file (`TargetFileType::ObjectFile`).
7. **Embedded Zero-Dependency Toolchain**:
   - On Windows x86-64, CCompiler contains its own native assembler and PE32+ linker, producing executable PE binaries without requiring external tools (`as`, `ld`, `gcc`, or `link.exe`).

---

## 2. Component Implementation

### Frontend (`src/frontend/`)

- **Lexer (`lexer.hpp`, `lexer.cpp`)**:
  - Tokenizes C source into `Token` objects (`TokenType`, `value`, `line`, `column`).
  - Supports identifier keywords (`int`, `char`, `short`, `long`, `struct`, `union`, `static`, `extern`, `if`, `else`, `while`, `for`, `do`, `switch`, `case`, `default`, `return`, `break`, `continue`, `goto`, `sizeof`, `asm`).
  - Supports multi-character operators (`++`, `--`, `&&`, `||`, `==`, `!=`, `<=`, `>=`, `<<`, `>>`, `->`).
  - Decodes integer literals (decimal, octal `0...`, hex `0x...`), character literals with escape sequences (`\n`, `\t`, `\0`, etc.), and string literals.
- **Preprocessor (`preprocessor.hpp`, `preprocessor.cpp`)**:
  - Handles `#define` (object-like and function-like macros), `#undef`.
  - Conditional compilation directives: `#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif`, including `defined(MACRO)`.
  - File inclusion: `#include <...>` (search include directories) and `#include "..."` (relative to current file).
  - Virtual header fallback for standard headers (`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<stdbool.h>`).
- **Parser (`parser.hpp`, `parser_core.cpp`, `parser_decl.cpp`, `parser_expr.cpp`, `parser_stmt.cpp`)**:
  - Recursive descent parser with modular source files:
    - `parser_core.cpp`: Parser initialization, token matching, error reporting.
    - `parser_decl.cpp`: Types, declarations, struct/union definitions, parameters, function declarations/definitions, `static`/`extern` specifiers.
    - `parser_expr.cpp`: Precedence-climbing binary expressions, unary operators (`*`, `&`, `!`, `~`, `-`, `+`, `sizeof`), function calls, array subscripting, struct member access (`.` and `->`).
    - `parser_stmt.cpp`: Compound statements, control flow, declarations inside blocks, labels, gotos, inline assembly blocks.
- **Abstract Syntax Tree (`ast.hpp`)**:
  - Complete hierarchy of nodes deriving from `ASTNode` with `ASTVisitor` support.

### Middle-End & Intermediate Representation (`src/ir/`)

- **Target Information (`target_info.hpp`)**:
  - `TargetInfo::get(TargetArch)` and `TargetInfo::from_string(arch_name, local_var_storage)`.
  - Defines `char_size`, `short_size`, `int_size`, `long_size`, `pointer_size`, `is_64bit`, and `is_little_endian`.
  - Resolves `get_effective_local_var_storage()` (`LocalVarStorage::Memory` for AVR by default, `LocalVarStorage::Stack` for 64-bit platforms).
- **Three-Address Code IR (`ir.hpp`, `ir.cpp`)**:
  - `IROpKind`: Operations covering `Assign`, `Add`, `Sub`, `Mul`, `Div`, `Mod`, `Neg`, `BitAnd`, `BitOr`, `BitXor`, `BitNot`, `Shl`, `Shr`, `Eq`, `Ne`, `Lt`, `Le`, `Gt`, `Ge`, `Load`, `Store`, `Alloca`, `Gep`, `Branch`, `BranchIf`, `Jump`, `Label`, `Call`, `Return`, `InlineAsm`.
  - `IROperand`: Strongly typed operands (`Constant`, `Variable`, `Temp` virtual register, `Label`, `Global`).
  - `IRFunction` & `IRModule`: Encapsulate lowered instructions, variables, parameters, and global variable allocations (`global_vars`).
- **Storage-Aware IR Builder (`ir_builder.hpp`, `ir_builder.cpp`)**:
  - Lowers AST into TAC IR based on the target's storage model:
    - **`stack` mode**: Local variables are allocated as function-local temporaries/registers (`%v0`).
    - **`memory` mode**: Local variables are allocated in static module memory (`global @_static_<func>_<var> : N bytes`). Variable updates emit `store @_static_...` and variable accesses emit `load @_static_...`.
  - Generates unique temporary registers (`%v0`, `%v1`, ...) and basic block labels (`.if_then_N`, `.while_start_N`, etc.).
  - Implements short-circuit evaluation for `&&` and `||`.
  - Lowers `switch` statements into condition-and-jump dispatch tables.
  - Automatically wraps top-level statements into a synthesized `main` function when no explicit `main` is provided.

### Backend & Code Generators (`src/backend/`)

- **Base Class & Factory (`codegen.hpp`, `codegen.cpp`)**:
  - `ICodeGenerator`: Abstract base class implementing the visitor pattern.
  - `CodeGeneratorFactory::create(arch, options)`: Instantiates target-specific generators.
  - Standardized `UnsupportedFeatureException` throwing when an architecture encounters an unsupported feature.
- **Local Variable Storage Models (`LocalVarStorage`)**:
  - `Stack`: Traditional frame pointer (`rbp` / `x29` / `Y`) based stack allocations.
  - `Memory`: Static SRAM variable allocations (crucial for 8-bit AVR microcontrollers with limited hardware stack).
  - `Auto`: Resolves to `Memory` for AVR, `Stack` for 64-bit architectures.

### Architecture-Specific Backends, Assemblers & Linkers (`src/backend/`)

#### 1. x86_64 (`src/backend/x86_64/`)
- **Code Generator (`codegen_x86_64.hpp`, `codegen_x86_64.cpp`)**:
  - Emits 64-bit AMD64 / Intel 64 assembly in AT&T syntax.
  - Supports Win64 ABI and SysV AMD64 ABI, stack-frame management, and calling convention modifiers.
- **Internal Assembler (`internal_assembler.hpp`, `internal_assembler.cpp`)**:
  - Line-by-line parser for AT&T syntax x86-64 assembly.
  - Directives: `.globl`, `.local`, `.section .text`, `.section .data`, `.section .rdata`, `.section .rodata`, `.asciz`, `.string`, `.quad`, `.long`, `.word`, `.byte`, `.space`, `.align`.
  - Encodes x86-64 machine instructions into byte streams: REX prefixes, ModR/M, SIB bytes, immediate encodings.
  - Emits `AssembledModule` containing raw section buffers, symbol offsets, and relocation records.
- **Embedded PE Linker (`internal_pe_linker.hpp`, `internal_pe_linker.cpp`)**:
  - Links one or more `AssembledModule` objects into a standalone Windows PE32+ (64-bit) executable or DLL.
  - Sections: `.text` (code), `.rdata` (constants, imports, ILT, IAT), `.data` (initialized globals).
  - Resolves internal relocations (`IMAGE_REL_AMD64_REL32`, `IMAGE_REL_AMD64_ADDR64`), cross-module symbols, and DLL imports (`kernel32`, `msvcrt`, `user32`).
- **Embedded Linux ELF Linker (`internal_elf_linker.hpp`, `internal_elf_linker.cpp`)**:
  - Links one or more `AssembledModule` objects into a standalone statically-executable Linux ELF64 (`ET_EXEC`) binary.
  - Generates standard `Elf64_Ehdr`, single `PT_LOAD` `Elf64_Phdr`, and section headers (`.text`, `.rodata`, `.shstrtab`).
  - Embeds standard Linux SysV ABI CRT startup code (`_start` at `0x401000` entry point).
  - Supplies built-in syscall-based standard I/O implementations (`puts`, `printf`, `putchar`, `exit`, `write`) with zero external dependencies (no glibc or dynamic linker required).

#### 2. AVR (`src/backend/avr/`)
- **Code Generator (`codegen_avr.hpp`, `codegen_avr.cpp`)**:
  - Emits 8-bit AVR RISC instructions targeting the ATmega328P microcontroller (Arduino Uno).
  - Supports both `--local-vars memory` (static SRAM allocation) and `--local-vars stack` (Y frame pointer).
- **AVR Assembler (`avr_assembler.hpp`, `avr_assembler.cpp`)**:
  - Encodes 8-bit AVR / ATmega328P RISC instructions into 16-bit and 32-bit Flash words:
    - Control flow: `rjmp`, `rcall`, `jmp`, `call`, `ret`, `reti`, `breq`, `brne`, `brlt`, `brge`, `brcs`, `brcc`.
    - Data transfer: `ldi`, `mov`, `movw`, `in`, `out`, `lds`, `sts`, `ld`, `st`, `ldd`, `std`, `push`, `pop`.
    - Arithmetic & logic: `add`, `adc`, `sub`, `sbc`, `subi`, `sbci`, `adiw`, `sbiw`, `and`, `andi`, `or`, `ori`, `eor`, `clr`, `com`, `neg`, `inc`, `dec`, `cp`, `cpc`, `cpi`.
    - Bit/IO instructions: `sbi`, `cbi`, `sbic`, `sbis`, `cli`, `sei`, `nop`.
  - Directives: `.global`, `.local`, `.section .text/.data/.bss`, `.byte`, `.word`, `.zero`, `.space`.
  - Emits `AVRAssembledModule` with Flash bytes, SRAM data/bss sizes, and relocations.
- **AVR Linker (`avr_linker.hpp`, `avr_linker.cpp`)**:
  - Target: **ATmega328P / Arduino Uno**.
  - Adds bare-metal CRT0 runtime:
    - Flash address `0x0000`: Reset vector (`jmp __init`).
    - `__init`: disables interrupts, clears `r1` (zero register), initializes stack pointer to `RAMEND` (`0x08FF`), and calls `main`.
  - Merges multiple modules with 2-byte word alignment and cross-module symbol resolution.
  - Relocates program memory word addresses (`pm_lo8`, `pm_hi8`, relative branch/jump offsets) and SRAM data addresses (`0x0100` base).
  - Emits an **Intel HEX (`.hex`)** flash image file ready for programming with `avrdude`.

#### 3. ARM64 (`src/backend/arm64/`)
- **Code Generator (`codegen_arm64.hpp`, `codegen_arm64.cpp`)**:
  - Generates standard 64-bit ARM AArch64 assembly text adhering to AAPCS64.
  - Manages hardware-enforced 16-byte stack pointer alignment, Frame Pointer (`x29`), and Link Register (`x30`).

#### 4. C Transpiler (`src/backend/c/`)
- **Transpiler (`codegen_c.hpp`, `codegen_c.cpp`)**:
  - Transforms AST structures into standard, portable ANSI C / C99 code.
  - Used for AST lowering validation, cross-compilation reference, and portability verification.

### Driver & Configuration (`src/driver/`)

- **Configuration (`compiler_config.hpp`, `compiler_config.cpp`)**:
  - `CompilerConfig`: Stores `general`, `preprocessor`, `codegen`, and `linker` settings.
  - `CommandLineParser`: Flexible POSIX and GNU style argument parser.
- **Driver Orchestration (`main.cpp`)**:
  - Directs translation units through Preprocessor, Parser, IR Builder, and Codegen.
  - Honors stop phases (`StopPhase::PreprocessOnly`, `StopPhase::IROnly`, `StopPhase::AssembleOnly`, `StopPhase::CompileOnly`, `StopPhase::FullBuild`).
  - Invokes `InternalPELinker` when targeting executable generation.

---

## 3. User Interface & CLI

### Options Reference

```
Usage: c_compiler [options] <source_files...> [libraries...]
```

| Flag | Parameter | Description | Default |
| :--- | :--- | :--- | :--- |
| **`-t, --target`** | `<spec>` | Target architecture and OS specification (`<arch>[-<os>]`) | Host default (`x86_64-windows` / `x86_64-linux`) |
| **`--type`** | `<type>` | Target file type (`auto`, `exe`, `obj`, `shared`) | `auto` |
| **`-o, --output`** | `<file>` | Output file name | `a.exe` (Windows) / `a.out` (Linux) / `a.hex` (AVR) |
| **`-c`** | — | Compile and assemble only; do not link (generates object modules) | — |
| **`-S`** | — | Stop after compilation; output assembly text (`.s`) | — |
| **`-E`** | — | Stop after preprocessing; output preprocessed source | — |
| **`--emit-ir, --ir`**| — | Stop after IR generation; output intermediate representation | — |
| **`--show-ir`** | — | Print intermediate representation to `stderr` during build | `false` |
| **`--ast`** | — | Print AST to `stdout` | `false` |
| **`-shared`** | — | Produce shared library / DLL | `false` |
| **`-v, --verbose`** | — | Verbose diagnostic output of compiler configuration | `false` |
| **`--local-vars`** | `<mode>` | Local variable storage (`stack`, `memory`, `auto`) | `auto` (`memory` for AVR, `stack` for others) |
| **`-I`** | `<dir>` | Add directory to include file search path | — |
| **`-D`** | `<macro>[=val]`| Define preprocessor macro (defaults to `1` if `=val` omitted) | — |
| **`-l`** | `<lib>` | Link library name (e.g. `-luser32`) | — |
| **`-L`** | `<dir>` | Add directory to library search path | — |
| **`-g, -g2, -g3`** | — | Emit source debug info (`.file` and `.loc` DWARF line mapping) | `true` (enabled by default) |
| **`-g0, --no-debug`** | — | Exclude source debug information | — |
| **`-s, --strip`** | — | Omit debug information and strip symbol tables | — |
| **`-h, --help`** | — | Print help and supported targets summary | — |

### Supported Targets (`-t, --target <spec>`)

The compiler distinguishes targets by **architecture**, **operating system**, **binary format**, and **calling convention / ABI**:

| Target Spec | Architecture | Operating System | Binary Format | Calling Convention / ABI | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `x86_64-windows` | `x86_64` | Windows | PE32+ (`.exe`, `.dll`) | Win64 (`rcx`, `rdx`, `r8`, `r9`, shadow space) | Windows 64-bit PE/COFF binary |
| `x86_64-linux` | `x86_64` | Linux | Static ELF64 (`ET_EXEC`) | System V AMD64 (`rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`) | Linux 64-bit static executable |
| `arm64-linux` | `arm64` | Linux | ELF64 / Asm | AAPCS64 (`x0`–`x7`, 16B SP align) | Linux 64-bit ARMv8-A (Raspberry Pi, etc.) |
| `avr-none` | `avr` | Bare-Metal | Intel HEX (`.hex`) | AVR Default (r24:r25 return, r1 zero) | Arduino Uno / ATmega328P flash firmware |
| `c-none` | `c` | None | Portable C Source | None | Portable ANSI C / C99 transpilation |

#### Target Format Syntax & Aliases
- Syntax: `-t <arch>[-<os>]`
- Common Aliases:
  - `x86_64`, `amd64` $\rightarrow$ Host platform default (`x86_64-windows` on Windows, `x86_64-linux` on Linux)
  - `linux`, `elf64` $\rightarrow$ `x86_64-linux`
  - `arm64`, `aarch64`, `rpi` $\rightarrow$ `arm64-linux`
  - `avr`, `arduino`, `atmega328p` $\rightarrow$ `avr-none`
  - `c`, `c_transpiler` $\rightarrow$ `c-none`

### Stop Phases

```
                ┌───────────────────────────────────────────────┐
 -E             │ Preprocessing only                            │
                ├───────────────────────────────────────────────┤
 --emit-ir      │ Preprocessing -> Parsing -> IR Generation     │
                ├───────────────────────────────────────────────┤
 -S             │ Preprocessing -> Parsing -> Code Generation   │
                ├───────────────────────────────────────────────┤
 -c             │ Preprocessing -> Parsing -> Codegen -> Asm    │
                ├───────────────────────────────────────────────┤
 (default)      │ Full build: Preprocessing -> Codegen -> Link  │
                └───────────────────────────────────────────────┘
```

### Usage Examples

1. **Compile and link a single C file to an executable:**
   ```bash
   c_compiler.exe main.c -o app.exe
   ```

2. **Compile multiple translation units together:**
   ```bash
   c_compiler.exe main.c math.c utils.c -o program.exe
   ```

3. **Inspect the Intermediate Representation (IR):**
   ```bash
   c_compiler.exe --emit-ir input.c
   ```

4. **Emit IR to a file:**
   ```bash
   c_compiler.exe --emit-ir input.c -o input.ir
   ```

5. **Generate assembly code for ARM64:**
   ```bash
   c_compiler.exe -t arm64 -S input.c -o input_arm64.s
   ```

6. **Generate AVR microcontroller assembly with static memory variables:**
   ```bash
   c_compiler.exe -t avr --local-vars memory -S firmware.c -o firmware.s
   ```

7. **Transpile C to clean ANSI C:**
   ```bash
   c_compiler.exe -t c -S input.c -o transpiled.c
   ```

8. **Link against Windows system DLLs:**
   ```bash
   c_compiler.exe gui.c -luser32 -o gui.exe
   ```

---

## 4. Target Architectures

### x86_64 (Windows x64 / AMD64)

- **Target Identifier**: `x86_64` (aliases: `x86-64`, `amd64`, `x64`).
- **Data Model**: LP64 / LLP64 (char: 1 byte, short: 2 bytes, int: 4 bytes, long: 8 bytes, pointer: 8 bytes). Little-endian.
- **Register Usage**:
  - `%rax`: Return value register, primary expression accumulator.
  - `%rcx`, `%rdx`, `%r8`, `%r9`: Integer/pointer argument registers (Win64 ABI).
  - `%rsp`: Stack pointer (must maintain 16-byte alignment before `call`).
  - `%rbp`: Frame pointer (base pointer for local variables).
  - `%r10`, `%r11`: Volatile scratch registers.
- **Executable Generation**:
  - Direct machine code emission via built-in assembler.
  - Generates PE32+ (64-bit Windows) binaries with valid COFF headers, optional header, `.text`, `.rdata`, and `.data` sections, and import tables.
  - Support for `__cdecl`, `__stdcall`, and `__fastcall` function modifier parsing.

### Linux ELF64 (x86_64)

- **Target Identifier**: `linux` (aliases: `x86_64-linux`, `elf64`, `elf`).
- **Data Model**: LP64 (char: 1 byte, short: 2 bytes, int: 4 bytes, long: 8 bytes, pointer: 8 bytes). Little-endian.
- **Calling Convention**: System V AMD64 ABI (first 6 arguments in `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`; return value in `%rax`; 16-byte stack alignment).
- **Executable Generation**:
  - Direct machine code emission via embedded `X86_64InternalAssembler`.
  - Links standalone static ELF64 executables via `InternalElfLinker`.
  - Built-in CRT runtime (`_start` entry point calling `main` and issuing `sys_exit` syscall 60).
  - Standalone syscall-based I/O implementations (`puts`, `printf`, `putchar`, `exit`, `write`).
  - Zero external dependencies: produces a fully self-contained static executable running immediately on any Linux distribution (Ubuntu, Alpine, Debian, Arch, WSL2).

### ARM64 / AArch64

- **Target Identifier**: `arm64` (aliases: `aarch64`, `arm`, `rpi`, `raspberrypi`).
- **Data Model**: LP64 (char: 1 byte, short: 2 bytes, int: 4 bytes, long: 8 bytes, pointer: 8 bytes). Little-endian.
- **Register Usage**:
  - `x0`: Return value, primary accumulator.
  - `x0`-`x7`: Function arguments (AAPCS64).
  - `x8`: Indirect result location register.
  - `x9`-`x15`: Temporary registers.
  - `x29`: Frame Pointer (FP).
  - `x30`: Link Register (LR) holding return addresses.
  - `sp`: Stack pointer (hardware enforced 16-byte alignment on memory access).
- **Output**: GNU `as` compatible assembly text.

### AVR (8-bit Microcontroller / Arduino)

- **Target Identifier**: `avr` (aliases: `arduino`, `atmega328p`).
- **Data Model**: 16-bit (char: 1 byte, short: 2 bytes, int: 2 bytes, long: 4 bytes, pointer: 2 bytes). Little-endian.
- **Register Usage**:
  - `r24:r25`: 16-bit accumulator and primary return value pair.
  - `r22:r23`, `r20:r21`, `r18:r19`: Function argument register pairs.
  - `r28:r29` (`Y` register): Frame pointer for stack frames.
  - `r26:r27` (`X` register), `r30:r31` (`Z` register): Memory pointers.
- **Storage Modes**:
  - Supports `--local-vars memory` for static SRAM allocations (default) and `--local-vars stack` for re-entrant frames.
- **Binary & Firmware Emission**:
  - Direct Intel HEX (`.hex`) flash images via built-in assembler and linker, ready for flashing to Arduino Uno via `avrdude`.
  - GNU `as` compatible assembly text output (`-S`).

### C Transpiler

- **Target Identifier**: `c` (alias: `c_transpiler`).
- **Purpose**: De-sugars and transpiles AST structures into readable, portable C source code.
- **Use Cases**: Useful for inspecting the frontend's interpretation of source constructs, verification against host compilers (GCC, Clang, MSVC), and running test suites on targets without a native code generator.

---

## 5. Roadmap & TODOs

The following roadmap items represent planned improvements and current architectural tasks:

### Middle-End & Intermediate Representation
- [ ] **Direct IR-to-Target Codegen**: Connect backend code generators (`codegen_x86_64`, `codegen_arm64`, `codegen_avr`, `codegen_c`) to consume `IRModule` directly rather than traversing the raw AST, eliminating duplicate expression handling.
- [ ] **Semantic Analysis & Type Checking Pass**: Implement a dedicated `TypeChecker` visitor that annotates AST/IR nodes, validates types, performs implicit widening conversions, and computes struct layouts *before* codegen.
- [ ] **Middle-End Optimizations**:
  - Constant folding and propagation.
  - Dead code elimination (DCE).
  - Redundant load/store elimination.

### Linker & Binary Formats
- [ ] **ELF64 Output**: Implement an ELF object file and executable emitter for Linux/SysV and ARM64 platforms.
- [ ] **True Object File Emission (`-c`)**: Output standard `.obj` (COFF) and `.o` (ELF) files to disk rather than keeping assembled modules in memory only.
- [ ] **Static Library Archive Reader**: Add archive parser for `.lib` and `.a` files to extract only referenced object members.

### Language & Frontend Enhancements
- [ ] **Variadic Functions**: Add support for `...` parameter lists and `va_list` / `va_start` / `va_arg` / `va_end`.
- [ ] **Floating-Point Types**: Support `float` and `double` arithmetic, registers (SSE/XMM on x86-64, VFP/NEON on ARM64).
- [ ] **Preprocessor `#line` Markers**: Emit `#line` directives into preprocessed code so compiler diagnostics accurately report original header filenames and line numbers.
- [ ] **Typedef & Enum Symbol Tables**: Complete full typedef alias resolution and enum constant promotion.

### Backend Enhancements
- [ ] **Strict ABI Conformance on x86-64**: Cleanly separate Windows x64 ABI calling convention from System V AMD64 ABI (Linux/macOS), removing the current dual-load fallback sequence.
- [ ] **AVR Flash Memory Literals**: Add support for `PROGMEM` / `lpm` instruction emission for constant strings located in Flash memory.
- [ ] **Scaled Pointer Arithmetic**: Scale pointer offsets by `sizeof(*ptr)` across all target code generators.
