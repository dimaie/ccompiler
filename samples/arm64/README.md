# ARM64 (AArch64) Samples

These samples demonstrate cross-compilation for 64-bit ARM architectures (Raspberry Pi 3/4/5, Apple Silicon, AWS Graviton, and ARM Linux) using **CCompiler**.

---

## 1. Generating ARM64 Assembly

To generate GNU `as` compatible ARM64 assembly:

```bash
./build/c_compiler.exe -t arm64 -S samples/arm64/hello_arm64.c -o hello_arm64.s
```

---

## 2. Emitting ARM64 IR

To inspect the lowered Three-Address Code intermediate representation:

```bash
./build/c_compiler.exe -t arm64 --emit-ir samples/arm64/hello_arm64.c
```
