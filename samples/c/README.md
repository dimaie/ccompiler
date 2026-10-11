# C Transpiler Samples

These samples demonstrate using the **C Transpiler** target (`-t c`). This target parses the input program, performs AST de-sugaring, and re-emits clean, portable C code.

---

## 1. Transpiling to Portable C

```bash
./build/c_compiler.exe -t c -S samples/c/transpiled_demo.c -o transpiled_out.c
```
