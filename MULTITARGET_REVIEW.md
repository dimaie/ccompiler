# CCompiler – Multi-Target Design & Implementation Review

**Scope:** `main.cpp`, `codegen.{hpp,cpp}`, `codegen_x86_64.cpp`, `codegen_avr.cpp`,
`codegen_arm64.cpp` (lines 1-330), `assembler_linker.cpp`, `internal_assembler.cpp`,
`internal_pe_linker.cpp`, `preprocessor.cpp`, `CMakeLists.txt`.
**Not re-read this time:** lexer/parser, `codegen_c.cpp`, `ast.hpp`, tests, the second half of `codegen_arm64.cpp`.

Severity: 🔴 wrong output / memory unsafety / invalid artifact · 🟠 incorrect or missing behaviour · 🟡 maintainability.

> **Relation to `CODE_REVIEW.md`:** that document predates the preprocessor, the internal
> assembler/PE linker and the AVR/ARM64 backends. Some of its items are now stale (e.g. x86_64 now
> tracks `var_pointer_level_`, frame size is computed after body generation, `-` stdin is accepted
> in `main.cpp:118`). Frontend findings there (lexer/parser/C backend) were not re-verified.

---

## 1. Architectural / design pitfalls

| # | Sev | Issue |
|---|---|---|
| A1 | 🔴 | **No semantic analysis / type system.** Each backend re-derives types ad hoc from the AST (`get_expr_type`, `get_expr_struct_type`, `var_types_`, `var_struct_name_`…). `get_expr_struct_type` is copy-pasted into x86_64, AVR and ARM64. Undeclared identifiers are silently treated as function addresses (`ldi pm_lo8(name)`, `leaq name(%rip)`) and `get_var_offset()` silently allocates storage for unknown names (all 3 backends). Typos compile "successfully". |
| A2 | 🔴 | **The same C source has different semantics per target.** `int` is 2 bytes on AVR, 8 bytes on x86_64/ARM64 (`get_type_size` returns 8), and the front end uses 64-bit literals (`IntLitNode.val`). AVR silently truncates literals (`static_cast<int16_t>` in `AVRCodeGenerator::visit(IntLitNode)`). No target-description object (sizeof int/long/pointer, alignment, endianness) exists; sizes are magic numbers (`2`, `8`) scattered through each backend. |
| A3 | 🟠 | **"Target" conflates ISA, ABI and OS.** `TargetArch::X86_64` means "x86-64 + Windows PE + a hybrid ABI". There is no way to ask for Linux/ELF or SysV. ARM64 emits GNU/ELF-style directives (`.type main, %function`) that don't work on Apple/Windows ARM. `C_Transpiler` is in the same enum as real architectures and is special-cased by string compare in `main.cpp:253`. |
| A4 | 🟠 | **Massive duplication across backends.** `generate()` (struct pre-pass, `has_func_defs/has_main_func/non_func_stmts`, synthesized-`main` path duplicated twice *inside each backend*), `compute_ast_frame_bytes`, `new_label`, `emit_line/emit_raw`, break/continue stacks, label/goto handling, `StructDef` layout, switch lowering. A bug fixed in one backend is not fixed in others (see §3, §4). Needs a shared base (`StackFrameCodeGenerator`) and a shared layout/sema pass. |
| A5 | 🟠 | **Direct AST → text emission, no IR.** No opportunity for shared optimization, register allocation, or target-independent checks (e.g. `break` outside loop is only detected in AVR). Every expression goes through a push/pop accumulator model on each target. |
| A6 | 🟠 | **Feature gaps are discovered late and inconsistently.** `ICodeGenerator` defaults to `UnsupportedFeatureException`, which is good, but there is no capability query, so `main.cpp` can't give an up-front, uniform error. Backends diverge: `StringLit` is unsupported on AVR, `long` rejected on AVR only, ≥7 args rejected on x86 / ≥5 on AVR / ≥9 on ARM64, inline `asm` is passed through verbatim with no validation. |
| A7 | 🟠 | **Two factories with duplicated, divergent alias tables.** `CodeGeneratorFactory::create(string)` and `AssemblerLinkerFactory::create(string)` each parse arch names. `x64` is accepted only by the linker factory; an unknown name falls back **silently** to `StubAssemblerLinker(X86_64, …)` instead of failing. `::tolower` on `char` (UB) at `assembler_linker.cpp:77`. |
| A8 | 🟡 | **Pipeline stage asymmetry.** x86_64 assembles/links in-process; AVR/ARM64/C stop at text and `main` returns **exit code 0** with only a "Notice" (`main.cpp:239-262`), so build scripts can't distinguish "executable produced" from "assembly only". `StubAssemblerLinker::assemble_and_link` is dead code (guarded by `is_supported()`) and reports `success=false` while still writing a file. |
| A9 | 🟡 | **`-t` is a free-form string** compared again in `main.cpp` (`target == "c"`), so alias `c_transpiler` gets a `.s` fallback filename. Use the parsed `TargetArch` enum throughout. |

---

## 2. Driver (`main.cpp`)

- 🟠 **Diagnostics point to wrong lines.** Included files are spliced into the preprocessed text with no `#line` markers (`preprocessor.cpp:555-566`), so lexer/parser/codegen line numbers (`UnsupportedFeatureException::line`) refer to the expanded buffer, not the user's file.
- 🟠 **Failure is not always fatal:** when `-o` can't be opened for the stub path (`main.cpp:243-246`) the error is swallowed and the program returns 0.
- 🟡 `--ast` writes the tree to stdout while the heading goes to stderr and generated code may also go to stdout → output interleaving.
- 🟡 `::tolower` on `char` (UB for negative values) at `main.cpp:96,120`.
- 🟡 Output file chosen for the stub path (`input.s` / `.transpiled.c`) silently overwrites existing files and ignores `-o`'s directory semantics. `-l`/`.a`/`.lib`/`.dll` heuristics are Windows-specific but accepted for every target (AVR, ARM64 ignore them silently).
- 🟡 `-o` default `a.exe` / `a.out` is chosen by **host** OS, but the x86_64 linker always emits a PE image; on Linux you get a PE file named `a.out`.

---

## 3. x86_64 backend (`codegen_x86_64.cpp`)

| # | Sev | Issue |
|---|---|---|
| X1 | 🔴 | **The "dual-load" call sequence (L500-505) is neither SysV nor Win64.** It overwrites `%rcx/%rdx/%r8/%r9` with copies of `rdi/rsi/rdx/rcx`. A SysV callee (Linux) now sees arg1 in `rdi` but arg3/arg4 in `rdx`/`rcx` overwritten with arg2/arg1 → wrong. For Win64, args 5 and 6 are left in `%r8/%r9` instead of the stack (and with `≥5` args `%r8/%r9` hold the *SysV* 5th/6th, not Win64's 3rd/4th since they're overwritten in a specific order). Only calls with ≤4 args to Win64 targets are correct. The comment claims both ABIs are supported; they aren't. |
| X2 | 🔴 | **Variadic calls**: Win64 requires no `%al`, SysV requires `%al` = #vector regs. `printf` works only by accident. The virtual `stdio.h` declares `int printf(const char *format);` (non-variadic), so the parser/type-checker can't express calls with extra args. |
| X3 | 🟠 | `Cdecl`/`Stdcall`/`Fastcall` on a 64-bit target are invented 8-byte-slot stack conventions, not any real ABI; they can't interoperate with external code and `fastcall` is neither Win64 `__fastcall` nor GCC's. Document, or reject outside 32-bit. |
| X4 | 🟠 | Globals are not emitted as data (no `.data/.bss`); with a user `main`, non-function top-level statements are dropped (`has_main_func` path only compiles `FuncDef`s). |
| X5 | 🟠 | `int`=8 bytes, pointer arithmetic unscaled, struct-by-value treated as one 8-byte register (see `CODE_REVIEW.md` for the older list; verify which are still open). |
| X6 | 🟡 | Frame size is computed correctly from `current_offset_` after generation (good), but a leftover `compute_ast_frame_bytes` still exists and is only used in the unreachable-by-design estimation path – delete or reuse. |
| X7 | 🟡 | Output ordering uses `ostringstream` splice (`param_ss`, `body_ss`) – fine, but `string_literals_` is emitted at the very end under `.section .rodata`, then `.text` is re-selected even if nothing follows. |

---

## 4. Internal assembler (`internal_assembler.cpp`)

The assembler is a hand-written *subset* of GNU `as` AT&T syntax. Its main flaw is that it **fails silently**.

| # | Sev | Issue |
|---|---|---|
| S1 | 🔴 | **Unknown mnemonics/operand forms emit nothing and raise no error.** The final `else if` chain has no `else`; e.g. `movl`, `cmpq mem`, `testq $imm`, `xchg`, `cmov*`, `seta/setb…`, `jb/ja/js…`, `sarq $imm,%reg`, `movq mem, mem`, `movzbl`, or any inline `asm` line simply vanishes → miscompiled binary with no diagnostic. Same for the RIP-relative branch (`L324`): only `leaq` is handled, `movq label(%rip), %rax` is silently dropped. |
| S2 | 🔴 | **Wrong encodings for unmatched operands.** `setcc` always encodes `%al` (`0F cc C0`) and unknown conditions default to `sete`; `jcc` unknown conditions default to `je` (`L409`); `movzbq` always encodes `%al→%rax` regardless of operands (`L227`); shifts ignore the count operand and always encode `%cl`. Registers like `%sil/%dil/%r8b`, `%xmm*` return `-1`, which is then passed to `make_rex/make_modrm` (`parse_reg` result unchecked everywhere). `movb`/`movw` stores omit REX handling for `r8+`. |
| S3 | 🔴 | **Function address of a function** `leaq fn(%rip), %rax` creates a `Rip_Data` relocation but the linker resolves `Rip_Data` only against `data_offsets` → **"unresolved data label"** for taking the address of a function (the AST backends emit exactly this in `IdentifierNode` decay). |
| S4 | 🟠 | Comment stripping `find_first_of(";#")` truncates `.string "a;b"` / `"#1"` literals (`L110`), and a `;`-separated multi-instruction line is not supported. `label: instr` on one line isn't supported (`line.back()==':'` only). Local labels are global; duplicate labels overwrite silently. |
| S5 | 🟠 | **No `.data`/`.bss`:** every non-`.text` section is "SecData" and is placed in a **read-only** `.rdata`; any writable global would fault. `.section` names other than `.rodata/.rdata` fall into `.text`. |
| S6 | 🟠 | `ops[0][0]` / `ops[1][0]` indexing on possibly-empty operand strings (UB when `op_str` is empty or a trailing comma exists). `std::stoll/stoi` can throw uncaught-in-context exceptions with unhelpful messages. Immediates > 32-bit for `addq/subq/cmpq` are truncated silently (`static_cast<int32_t>`). |
| S7 | 🟡 | Always uses 10-byte `movabs` for `movq $imm,%reg` and rel32 for every jump; no relaxation. No instruction/line numbers in errors because there are no errors. |

**Recommendation:** make every unmatched path `throw std::runtime_error("unsupported instruction: <line>")`, validate `parse_reg != -1`, and add a round-trip test that assembles everything the x86 backend can emit and compares against `as`/`objdump`.

---

## 5. PE linker (`internal_pe_linker.cpp`)

| # | Sev | Issue |
|---|---|---|
| L1 | 🔴 | **Unresolvable imports become runtime load failures.** Every external symbol with no known DLL defaults to `msvcrt.dll` (L414-419) without checking it exports the symbol; typos produce an exe that fails at load time instead of a link error. |
| L2 | 🔴 | **Unaligned/unchecked binary parsing.** `read_coff_archive` does `*reinterpret_cast<const uint16_t*>` on arbitrary buffer offsets (UB, alignment); `mem_off + 60` and `num_syms * 4` are unchecked/can overflow; `name_str.rfind("/",0)==0` matches *every* long-name member (`/123`), so non-linker members are parsed as symbol tables. `read_dll_exports` trusts `pe_offset`, `num_names` (allocates `vector<uint32_t>(num_names)` from file data) and ignores forwarded exports/ordinals. |
| L3 | 🟠 | Only one hardcoded layout (2 sections, no `.data/.bss`, no `.reloc`, `DllCharacteristics=0`, fixed `ImageBase`, no relocation of absolute addresses). Writable data and `main(argc, argv)` aren't possible; `mainCRTStartup` calls `main` with no args. |
| L4 | 🟠 | Thunks are emitted for **every** external symbol even if unused; `ExitProcess` is always imported; hardcoded `C:\Windows\System32` search path; `Rip_Data` relocations are only text→data (no data→data/code, no absolute 64-bit relocations, so jump tables / function-pointer tables in data can't work). |
| L5 | 🟡 | `write` failures are checked, but a partially written output remains on error; no `chmod +x`, no checksum, `SizeOfStackReserve` fixed at 1 MiB. Windows-only linker is compiled into all builds without an `#ifdef`. |

---

## 6. AVR backend (`codegen_avr.cpp`)

| # | Sev | Issue |
|---|---|---|
| V1 | 🔴 | **Frame addressing exceeds AVR instruction limits.** `std/ldd Y+q` requires `q ≤ 63`, `sbiw/adiw` immediates ≤ 63, and `adiw r30, off` ≤ 63. Frames are `max(32, extra+2)` bytes with no upper bound; a function with >~30 locals or a 40-element `int[]` generates assembly the assembler will reject (or, via `subi`, access wrong data). No fallback to Z/pointer-based addressing. |
| V2 | 🔴 | **Comparisons are wrong.** `<=` is compiled as `brlt` and `>` as `brge` ("approximation", L849-851): `a <= b` is false when `a == b`, `a > b` is true when `a == b`. Unsigned/`char` not distinguished (`brsh/brlo`). |
| V3 | 🔴 | **`/` and `%` generate only a comment** (L828-833) and fall through, returning garbage (the right operand) with no diagnostic. Must `unsupported()` or call a helper (`__divmodhi4`). |
| V4 | 🔴 | **Stack-frame estimate ignores struct sizes**: `compute_ast_frame_bytes` counts 2 bytes for every scalar/struct, `array_size*2` for arrays regardless of element type, and ignores `StructDef.var_decl` (x86 handles that). Struct locals overflow the allocated frame into the caller's stack. Same defect exists in ARM64 (see below). |
| V5 | 🟠 | `ArrayAccess`/array assign on a **pointer variable** (`int *p; p[i]`) treats `p`'s own stack slot as array storage (`ArrayAccessNode` path always computes `Y + base_offset + 2*i`) – parameters like `int a[]`/`int *a` are mis-indexed. Element size hard-coded to 2 even for `char`/struct arrays. |
| V6 | 🟠 | Scalar vs array/struct slots use **two different offset conventions** (`get_var_offset` returns the *high*-byte offset: `Y+off-1,Y+off`; arrays/structs store `current_offset_+1` = low byte). Works today only because each access path knows its kind; any new path (member of array element, struct param) will silently be off by one. `var_struct_name_` is also not cleared per function (leaks types between functions). |
| V7 | 🟠 | Missing: `char`/8-bit, `long`, string literals/PROGMEM, `&&`-short-circuit is correct but `||`/`&&` use `sbiw` (r24,r25 only; fine); no bitwise/shift codegen reachable; no ISR/vector table/`.init` startup, `.arch atmega328p` is not a valid `avr-as` directive (use `-mmcu`), no `.text`/`.section` directives. Stub linker means output can't be built here. |
| V8 | 🟡 | The 9-instruction SP-update prologue/epilogue is copy-pasted three times (`generate`, synthesized `main`, `FuncDef`), `main` doesn't set up the `r1` zero register contract after `mul` consistently, and `cli/out` sequence is duplicated. Division of labour with avr-gcc ABI (callee-saved r2-r17) isn't enforced – `r26/r27` clobber in multiply is OK, but `r18-r27` use across calls to user code is unchecked. |

---

## 7. ARM64 backend (`codegen_arm64.cpp`, first 330 lines reviewed)

| # | Sev | Issue |
|---|---|---|
| R1 | 🔴 | **`stp x29, x30, [sp, -N]!` / `ldp … [sp], N` immediates are limited to −512…504 (multiple of 8).** Frames are `max(64, align16(extra+16))` with no cap → any function with more than ~60 8-byte slots emits invalid assembly. |
| R2 | 🔴 | Same **frame-size estimation defect** as AVR (`compute_ast_frame_bytes` counts 8 per struct/ignores sizes and `StructDef.var_decl`) → struct locals overflow into saved `x29/x30`. Locals start at `[x29, #16]` with the frame pointer at the bottom, so overflow clobbers the caller's frame. |
| R3 | 🟠 | Every arg is saved with `str x0,[sp,-16]!` then reloaded: correct but O(n) memory traffic; callee-saved registers (`x19-x28`) are never used so nothing else is wrong, but there is no alignment guarantee when expression temporaries are pushed in odd counts (they always push 16 bytes → ok). |
| R4 | 🟠 | Stub linker only: no assembler, no linking, so ARM64 output is never validated by the test-suite beyond string compares (`test_codegen.cpp`). Variadic calls on Apple ARM64 pass varargs on stack – unsupported; `.type` directive not valid on Mach-O. |
| R5 | 🟡 | `int` semantic = 64-bit (A2), `get_type_size` returns 8 for everything except `char/short`, pointers arithmetic unscaled (shared with x86). |

---

## 8. Preprocessor (`preprocessor.cpp`)

| # | Sev | Issue |
|---|---|---|
| P1 | 🔴 | **Empty object-like macros expand to `1`** (L518-519: `if (def.replacement.empty()) def.replacement = "1"`). `#define EXPORT` / `#define STATIC` then `STATIC int x;` becomes `1 int x;`. Only the *command-line* `-D` form should default to 1. |
| P2 | 🔴 | **Comments aren't handled before directive/line processing.** A `#define`/`#include`/`#if` inside a multi-line `/* … */` comment is executed; multi-line comments are macro-expanded line by line with no state; `//` comments that end with `\` are joined with the next line. |
| P3 | 🟠 | **Line continuation join happens first and drops newlines**, so line numbers after a `\`-continued line shift (the code preserves line count for inactive `#if` blocks, but not for continuations or includes). Combined with no `#line`, all diagnostics are unreliable. |
| P4 | 🟠 | Function-like macros: no `#`/`##`, no variadics/`__VA_ARGS__`, arguments are not pre-expanded, arguments with `,`/`)` inside char literals or an escaped backslash before a quote (`text[peek-2] != '\\'`) break splitting, invocations spanning lines don't work, arity mismatch isn't diagnosed, unterminated invocation silently expands with partial args. Macro table is shared across `-D` and includes (no `__FILE__`, `__LINE__`, `__STDC__`, `__AVR__/__x86_64__` target macros are predefined – **no per-target predefined macros at all**, which a multi-target compiler needs). |
| P5 | 🟠 | `#if` evaluator supports only `|| && == != < <= > >= + - ! ()`; missing `* / % << >> & | ^ ~ ?:`, character literals, integer suffixes; unknown tokens are skipped with `pos_++` and evaluate to 0 silently (`#if X/2` → wrong result, no error). Unbalanced `#if` at EOF, duplicate `#else`, and `#elif` after `#else` aren't diagnosed. |
| P6 | 🟠 | Unknown directives (`#error`, `#warning`, `#pragma`, `#line`, `#include_next`) are not handled: `#error` doesn't stop compilation, `#pragma once` isn't supported (only 64-level depth cap protects from recursion), unknown directives fall through to the lexer as `TOK_HASH` with a confusing parse error. |
| P7 | 🟠 | **Include resolution is CWD-dependent and non-deterministic:** after `-I` dirs it tries `include`, `../include`, `../../include` relative to the *current working directory* before falling back to the built-in virtual headers, so output changes depending on where the compiler is run. Virtual `stdio.h` declares non-variadic `printf` and `stdlib.h`/`string.h` use `unsigned long`, which the parser may not accept (verify). Headers are re-read/re-processed on every include (no cache). |
| P8 | 🟡 | `include_depth_` is not decremented when `process_internal` throws (reset only in `process`). `std::set<std::string> active_macros` is allocated per line; whole-file string concatenation per line is O(n²) for large inputs. |

---

## 9. Build / test infrastructure (`CMakeLists.txt`)

- 🟠 Every executable recompiles all sources (`PARSER_SOURCES`, `CODEGEN_SOURCES`) instead of using a static library target; three copies of the whole compiler are built.
- 🟠 `add_link_options(-static -static-libgcc -static-libstdc++)` is applied to **all** GCC/Clang builds, which breaks on macOS/Clang and on glibc systems lacking static libs. Gate on MinGW.
- 🟡 No sanitizer / `-Werror` options; no per-target feature test (e.g. "can assemble `.s` with `avr-as`/`aarch64-linux-gnu-as` if available") so AVR/ARM64 output is only compared as text.
- 🟡 Build artifacts (`hello.exe`, `out.c`, `input.c`, `build/`, `.cache/`) are in the workspace root – confirm `.gitignore` excludes them.
- 🟡 Tests (`test_codegen.cpp` 51 KB, `test_parser.cpp` 55 KB) are monolithic; no golden-file/executable tests for the x86_64 → PE path (the only end-to-end target).

---

## 10. Prioritised recommendations

1. **Make the internal assembler strict** (S1–S3) – silent instruction dropping is the most dangerous bug in the whole pipeline, and fix function-address relocation (S3).
2. **Fix the x86_64 call ABI** (X1): choose Win64 *or* SysV per an explicit `Target{arch, os, abi}` description; delete the dual-load hack.
3. **Introduce a `TargetInfo` + shared semantic pass** (A1, A2, A4): type sizes, symbol table (errors on undeclared names), struct layout, frame layout; remove the three copies of `compute_ast_frame_bytes`/`get_expr_struct_type`/`generate()` scaffolding.
4. **Fix backend correctness bugs:** AVR comparisons (V2), division (V3), frame limits (V1/V4); ARM64 frame limits (R1/R2).
5. **Preprocessor:** empty macros (P1), comment handling (P2), `#line` markers (P3), `#error`/`#pragma once` (P6), target predefined macros (P4).
6. **Driver:** use the `TargetArch` enum, return a distinct status when only assembly was produced, host-independent output naming (§2, A8, A9).
7. **Linker:** validate imports (L1) and harden archive/DLL parsing (L2).
8. Restructure CMake into a library + tools; add golden-file tests and, when toolchains are present, assemble-and-run tests (e.g. `avr-as`, `qemu-aarch64`, run the PE on Windows).
