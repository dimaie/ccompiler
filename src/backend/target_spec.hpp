#ifndef TARGET_SPEC_HPP
#define TARGET_SPEC_HPP

#include <iostream>
#include <string>
#include <vector>

/**
 * @brief CPU Architecture families supported by the compiler.
 */
enum class TargetArch {
  X86_64,
  AVR,
  ARM64,
  C_Transpiler
};

/**
 * @brief Operating Systems and runtime environments.
 */
enum class TargetOS {
  Windows,      // Windows (PE32+, Win64 ABI, system DLLs)
  Linux,        // Linux (ELF64, SysV AMD64 / AAPCS64 ABI, Linux syscalls)
  BareMetal,    // Bare-metal microcontrollers (AVR / Harvard architecture, raw memory)
  None          // Source transpilation or OS-agnostic
};

/**
 * @brief Output binary / artifact formats.
 */
enum class TargetFormat {
  PE,           // Windows Portable Executable (.exe, .dll)
  ELF,          // Linux Executable and Linkable Format (.out, static ELF64)
  IntelHex,     // Flash firmware image (.hex) for microcontrollers
  AssemblyOnly, // Assembly text (.s)
  C_Source      // ANSI C / C99 source (.c)
};

/**
 * @brief Application Binary Interface (ABI) conventions.
 */
enum class TargetABI {
  Win64,        // Microsoft x64 (rcx, rdx, r8, r9, 32B shadow space)
  SysV_AMD64,   // System V AMD64 (rdi, rsi, rdx, rcx, r8, r9)
  AAPCS64,      // ARM 64-bit Architecture Procedure Call Standard (x0-x7)
  AVR_Default,  // 8-bit AVR calling convention (r24:r25, r22:r23, etc.)
  None
};

const char *target_arch_str(TargetArch arch);
const char *target_os_str(TargetOS os);
const char *target_format_str(TargetFormat fmt);
const char *target_abi_str(TargetABI abi);

/**
 * @brief Comprehensive target specification encapsulating Architecture, OS,
 *        Binary Format, and ABI conventions.
 */
struct TargetSpec {
  TargetArch arch{TargetArch::X86_64};
  TargetOS os{TargetOS::Windows};
  TargetFormat format{TargetFormat::PE};
  TargetABI abi{TargetABI::Win64};
  std::string name{"x86_64-windows"};

  TargetSpec() = default;
  TargetSpec(TargetArch a, TargetOS o, TargetFormat f, TargetABI b, std::string n)
      : arch(a), os(o), format(f), abi(b), name(std::move(n)) {}

  /// Parse a target triple or alias (e.g. "x86_64", "x86_64-linux", "arm64-linux", "avr", "c")
  static TargetSpec parse(const std::string &str);

  /// Canonical target representation string (e.g. "x86_64-windows", "x86_64-linux", "arm64-linux")
  std::string to_string() const;

  /// Default output executable / binary filename for this target
  std::string default_output_filename() const;

  /// Default file extension for this target's binary output
  std::string default_extension() const;

  /// Formatted target reference table for CLI --help output
  static void print_supported_targets(std::ostream &out);
};

#endif // TARGET_SPEC_HPP
