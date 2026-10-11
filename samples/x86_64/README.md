# x86_64 (Windows x64) Samples

These samples demonstrate building native Windows 64-bit PE binaries using **CCompiler**'s embedded x86-64 assembler and PE linker (no external toolchain required).

---

## 1. Console Hello World

Compile to a native 64-bit `.exe`:

```bash
./build/c_compiler.exe samples/x86_64/hello.c -o hello.exe
./hello.exe
```

---

## 2. Win32 GUI MessageBox

Compile and link against `user32.dll`:

```bash
./build/c_compiler.exe samples/x86_64/gui_msgbox.c -luser32 -o msgbox.exe
./msgbox.exe
```
