# CCompiler – Code Review

**Reviewed in full:** `main.cpp`, `lexer.cpp`, `parser_core.cpp`, `parser_expr.cpp`, `parser_stmt.cpp`, `parser_decl.cpp`, `codegen.cpp`, `codegen.hpp`, `codegen_c.cpp`, `codegen_x86_64.cpp`, `CMakeLists.txt`.
**Not reviewed in depth:** `codegen_avr.cpp`, `codegen_arm64.cpp`, `ast.hpp`, `lexer.hpp`, `parser.hpp`, tests.

Severity: 🔴 wrong output / memory unsafety · 🟠 incorrect behaviour or missing feature · 🟡 minor / maintainability.

---

## 1. Summary

The architecture is clean: lexer → recursive-descent parser → AST → visitor-based backends, with `ICodeGenerator` defaulting to `UnsupportedFeatureException` so new nodes need no work in other backends. The most serious problems are in the **C transpiler backend** (emits invalid C in common cases) and the **x86_64 backend** (no type system, so pointer/short/struct handling is wrong or corrupts memory). There are also several lexer edge cases and a few gaps between what the lexer tokenizes and what the parser accepts.

---

## 2. Findings

### 🔴 Critical

| # | File / Location | Issue |
|---|---|---|
| 1 | `codegen_c.cpp` `visit(IfNode/WhileNode/ForNode)` + statement loops | Non-compound bodies get no `;`. `if (x) y = 1; else z = 2;` becomes `if (x) y = 1 else z = 2;` and `while (c) x = 1;` loses its `;` because `If/While/For` are in the "no semicolon" list. Output is invalid C. |
| 2 | `codegen_c.cpp` `visit(UnaryOpNode)` | `-(-x)` prints `--x` (a decrement); `-(-1)` prints `--1`. Wrap unary results in parentheses or emit a space. |
| 3 | `codegen_c.cpp` `generate` | When any function exists, global variable declarations (non-`FuncDef`/`StructDef` statements) are silently **dropped**, even when `main` exists. |
| 4 | `codegen_c.cpp` `generate` | `StructDef` with an attached `var_decl` is emitted at top level (loop at L38–43) **and again** inside the synthesized `main` → duplicate definition. |
| 5 | `codegen_c.cpp` `visit(WhileNode/IfNode/ForNode)` | Empty-statement bodies (`while (x);`, parser returns `nullptr`) emit nothing after `)` → invalid C. |
| 6 | `codegen_x86_64.cpp` `compute_ast_frame_bytes` | Counts every scalar as 8 bytes and structs as 8 bytes, ignoring struct size and struct-array element size. Large struct locals overflow the (min. 128 byte) frame → **stack corruption**. Also doesn't count implicitly-allocated variables (`get_var_offset` auto-allocates) or `StructDef.var_decl`. |
| 7 | `codegen_x86_64.cpp` identifier/pointer handling | `var_types_` stores only `DataType`, not pointer level. A `short *p` is loaded with `movswq` (truncated to 16 bits) in `visit(IdentifierNode)` and assigned with `movw`. Pointers to `short` are corrupted. |
| 8 | `codegen_x86_64.cpp` `visit(UnaryOpNode)` `*p` / `*p = v` | Always 8-byte load/store regardless of pointee type → reading/writing `short *` touches 6 extra bytes (corrupts neighbours). |
| 9 | `codegen_x86_64.cpp` `visit(ArrayAccessNode)` / assignment | Always uses `leaq -off(%rbp)` as the base, i.e. only works for local arrays. `p[i]` where `p` is a pointer or array parameter indexes the *pointer variable's own storage* instead of the pointee. Struct arrays use scale 8 instead of the struct size. |
| 10 | `lexer.cpp` identifier/number scanning | If a token exceeds `sizeof(tok.text)-1`, scanning stops silently and the remaining characters become a **new token** (e.g. a long identifier splits in two). Should diagnose or consume the rest. |
| 11 | `lexer.cpp` `init(std::string_view)` | Calls `init(source.data())`, which requires NUL termination; unsafe for non-terminated views. |

### 🟠 Important

**main.cpp**
- `-` (stdin) is documented in code (`input_filename == "-"`) but unreachable: the argument parser rejects it as an unknown option because it starts with `-`.
- Output file is opened *before* generation; if codegen throws, an empty/partial file is left behind. Generate to a string first, then write.
- `--ast` prints to stdout, mixing with generated code when `-o` isn't used.
- No check that writing to the output file succeeded.

**Lexer**
- Lexes `++`, `--`, `<<`, `>>`, `|`, `&`, `^`, `~`, `->` but the parser has no support for `++/--`, shifts, bitwise binary ops, compound assignment (`+=` lexes as `+` `=`), `?:`, `sizeof`, casts, comma operator. Inputs using them yield "Expected expression"/confusing errors.
- Keywords `char`, `unsigned`, `signed`, `float`, `double`, `const`, `static`, `typedef`, `enum`, `extern`, … are tokenized but not parseable (`is_type_specifier` only accepts `int/short/long/void/struct/union`).
- Numeric literals: no `u`/`l` suffixes, no octal validation (`09`), no overflow detection, no float literals (`TOK_FLOAT_LIT` never produced).
- String/char literals: parsed by lexer but `parse_primary` doesn't accept `TOK_STRING_LIT`/`TOK_CHAR_LIT`. Unterminated string/char literals and unterminated `/* */` comments are accepted silently. Escape handling lacks `\x`, `\a`, `\b`, `\f`, `\v`, octal; unknown escapes pass through.
- `peek(k)` clamps `k` to `LOOKAHEAD_BUFFER` silently, returning the wrong token rather than failing.
- `#` preprocessor lines are tokenized as `TOK_HASH` but never handled.

**Parser**
- Function prototypes (`int f(int);`) are unsupported; `parse_function_def` always requires a body. Parameter names are mandatory; array parameters (`int a[]`) unsupported.
- Multiple declarators (`int a, b;`) unsupported.
- Function-pointer declarations drop pointer stars in parameter types (`void (*fp)(int *)` → params recorded as `int`), so the C backend prints `void (*fp)(int)`. `func_ptr_ret_pointer_level` is never set by the parser (`int *(*fp)(void)` fails to parse: stars consumed before `(`).
- `parse_var_decl` (used by `for` init) for a `struct S {…} v;` returns only the variable and discards the struct definition.
- Anonymous nested `struct { … } member;` inside structs is unsupported; function-pointer members unsupported.
- `parse_type_specifier` accepts odd combinations (`int long long`, `int short`).
- Function definitions are accepted in any statement position (nested functions) because `parse_statement` → `parse_declaration`.
- A label (`foo:`) must be followed by a statement per C, but parser returns the label alone; `case` bodies etc. are fine.
- Assignment l-value error is reported at the token *after* `=` (`peek()`), not at the offending expression.
- Any primary followed by `(` becomes a call, e.g. `(a)(b)` — intended for function pointers, but also affects things like `1 (2)`; no semantic rejection.
- `parse_while`/`parse_do_while`/`parse_switch` don't use `parse_parenthesized`, unlike `parse_if` (inconsistency).

**x86_64 backend**
- No semantic analysis: undeclared identifiers silently become function addresses (`leaq name(%rip)`), and `get_var_offset` silently allocates storage for unknown variables.
- Scopes are flat per function: shadowed variables share a name→offset entry; `var_types_` isn't restored on scope exit.
- `int` is 8 bytes (`get_type_size` returns 8 for everything except `short`); struct layouts / arrays differ from real C ABI, breaking interop with libc structs.
- Pointer arithmetic is not scaled (`p + 1` adds 1).
- Variadic calls (e.g. `printf`) don't set `%al`; required by the SysV ABI.
- Uses SysV argument registers (`rdi, rsi…`) and no underscore symbols; on Windows (Win64 ABI / `rcx, rdx`) output won't link/run correctly. Worth documenting target OS.
- Struct passed/returned by value silently treated as one 8-byte register.
- Global variables in a program that has `main` are ignored (only synthesized-main path emits them), and globals are emitted as stack locals rather than `.data`/`.bss`.
- Functions without `return` leave `%rax` undefined (fine in C for non-main, but `main` should return 0).

### 🟡 Minor / Maintainability

- `CCodeGenerator`: the "does this statement need a `;`" list is copy-pasted 4× (`generate`, `visit(Program)`, `visit(Compound)`, `visit(Case)`) with subtly different member lists (`StructDef` handled in some, not others → `};\n;`). Factor into one helper.
- `codegen.cpp`: `::tolower` on `char` is UB for negative values; cast to `unsigned char`. `switch` in `create(TargetArch)` has a `default` that hides `-Wswitch` warnings.
- `parser_decl.cpp`: heavy duplication between `parse_func_ptr_decl` and the function-pointer parameter path in `parse_function_def`; extract helper.
- `parse_type_specifier` has no `return` after `parse_error`; this relies on `parse_error` being `[[noreturn]]` — verify the declaration in `parser.hpp` to avoid `-Wreturn-type` warnings/UB.
- `X86_64CodeGenerator::visit(UnaryOpNode)` uses `member_map.at()` (throws `std::out_of_range`) instead of a diagnostic for unknown members in `&s.m`.
- `is_func_ptr_[name]` uses `operator[]`, inserting entries as a side effect of reads.
- `CMakeLists.txt`: no `-Werror`/sanitizer option; consider an ASan/UBSan debug config (would catch the `string_view` and buffer issues).
- Naming: `ICodeGenerator::unsupported` is both overloaded and virtual+`[[noreturn]]`; fine, but the `(string, int)` vs `(node, string)` overloads are easy to mix up.

---

## 3. Suggested fix order

1. C backend: always terminate statements (central `emit_statement()` helper), parenthesize unary ops, emit globals, avoid duplicate struct output, handle null bodies.
2. x86_64: track `pointer_level` per variable and pointee size; fix array base for pointers; correct frame-size computation (use real struct/array sizes or compute frame from allocated offsets after generation).
3. Lexer: diagnostics for over-long tokens, unterminated literals/comments; fix `string_view` init.
4. `main.cpp`: stdin handling, write output only on success.
5. Parser: wire up already-tokenized features (`++/--`, compound assign, bitwise/shift, string/char literals, prototypes, `char`/`unsigned`).
6. Add tests for each fixed bug in `test_codegen.cpp` / `test_parser.cpp`.
