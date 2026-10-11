# CCompiler Architecture Samples

This directory provides working sample code for every architecture supported by **CCompiler**:

| Directory | Target Architecture | Description |
| :--- | :--- | :--- |
| **[`samples/linux/`](linux/)** | `linux` (x86_64 ELF64) | Standalone native Linux 64-bit ELF executable with zero external dependencies. |
| **[`samples/avr/`](avr/)** | `avr` (ATmega328P / Arduino Uno) | Bare-metal Pin 13 LED blink program compiling directly to an Intel HEX (`.hex`) flash image. |
| **[`samples/x86_64/`](x86_64/)** | `x86_64` (Windows x64) | Console `puts()` and Win32 GUI `MessageBoxA` samples linking directly into native Windows PE `.exe` binaries. |
| **[`samples/arm64/`](arm64/)** | `arm64` (AArch64 / Raspberry Pi) | 64-bit ARM function calling conventions and assembly emission. |
| **[`samples/c/`](c/)** | `c` (ANSI C / C99 Transpiler) | AST de-sugaring and transpilation to clean, portable C code. |

---

## Quick Build Commands

```bash
# 1. Linux -> Standalone Native 64-bit ELF Executable
./build/c_compiler.exe -t linux samples/linux/hello_linux.c -o hello_linux

# 2. AVR Blink -> Intel HEX for Arduino Uno
./build/c_compiler.exe -t avr -I samples/avr samples/avr/blink.c -o samples/avr/blink.hex

# 3. x86-64 -> Native Windows Console Binary
./build/c_compiler.exe samples/x86_64/hello.c -o hello.exe

# 4. x86-64 -> Native Windows GUI Binary (linking user32.dll)
./build/c_compiler.exe samples/x86_64/gui_msgbox.c -luser32 -o msgbox.exe

# 5. ARM64 -> GNU as Compatible Assembly
./build/c_compiler.exe -t arm64 -S samples/arm64/hello_arm64.c -o hello_arm64.s

# 6. C Transpiler -> Portable C
./build/c_compiler.exe -t c -S samples/c/transpiled_demo.c -o transpiled.c
```
