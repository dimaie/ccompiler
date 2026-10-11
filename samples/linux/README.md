# Linux x86_64 ELF Samples

This directory demonstrates building native, standalone 64-bit Linux executables (ELF64 format) using **CCompiler**'s embedded toolchain:

- **Target Architecture**: `x86_64` (AMD64 / Intel 64)
- **Target OS / Format**: Linux ELF64 (`ET_EXEC`, SysV AMD64 ABI)
- **Zero External Dependencies**: The embedded `InternalElfLinker` generates a 100% self-contained static executable with built-in runtime (`_start`, `puts`, `putchar`, `printf`, `exit`, `write`) that runs on any Linux distribution (Ubuntu, Debian, Alpine, Fedora, Arch, WSL2) without needing glibc or an external dynamic linker.

---

## 1. Compiling Hello World for Linux

Run `c_compiler` with target flag `-t linux`:

```bash
# From repository root:
./build/c_compiler.exe -t linux samples/linux/hello_linux.c -o hello_linux
```

This performs the complete compilation, assembly, and linking sequence:
1. **Frontend**: Preprocesses `hello_linux.c` and parses AST.
2. **IR & Codegen**: Generates SysV AMD64 assembly instructions.
3. **Internal Assembler**: Encodes x86_64 machine code and records symbol relocations.
4. **Internal ELF Linker**:
   - Inserts standard Linux SysV CRT startup runtime (`_start` at `0x401000` entry point).
   - Injects standalone syscall-based standard I/O implementation (`puts` via `sys_write`).
   - Resolves 32-bit PC-relative code relocations and RIP-relative data relocations.
   - Formats and writes standard `Elf64_Ehdr`, `Elf64_Phdr`, `.text`, `.rodata`, and section headers.

---

## 2. Running on Linux / WSL2

Make the binary executable and run:

```bash
chmod +x ./hello_linux
./hello_linux
```

Output:
```text
Hello from CCompiler on Linux!
```
