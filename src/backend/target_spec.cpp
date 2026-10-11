#include "target_spec.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

const char *target_arch_str(TargetArch arch) {
  switch (arch) {
  case TargetArch::X86_64: return "x86_64";
  case TargetArch::ARM64: return "arm64";
  case TargetArch::AVR: return "avr";
  case TargetArch::C_Transpiler: return "c";
  default: return "unknown";
  }
}

const char *target_os_str(TargetOS os) {
  switch (os) {
  case TargetOS::Windows: return "windows";
  case TargetOS::Linux: return "linux";
  case TargetOS::BareMetal: return "none";
  case TargetOS::None: return "none";
  default: return "unknown";
  }
}

const char *target_format_str(TargetFormat fmt) {
  switch (fmt) {
  case TargetFormat::PE: return "pe";
  case TargetFormat::ELF: return "elf";
  case TargetFormat::IntelHex: return "hex";
  case TargetFormat::AssemblyOnly: return "asm";
  case TargetFormat::C_Source: return "c";
  default: return "unknown";
  }
}

const char *target_abi_str(TargetABI abi) {
  switch (abi) {
  case TargetABI::Win64: return "win64";
  case TargetABI::SysV_AMD64: return "sysv";
  case TargetABI::AAPCS64: return "aapcs64";
  case TargetABI::AVR_Default: return "avr";
  case TargetABI::None: return "none";
  default: return "unknown";
  }
}

std::string TargetSpec::to_string() const {
  std::string s = target_arch_str(arch);
  if (os != TargetOS::None) {
    s += "-";
    s += target_os_str(os);
  }
  return s;
}

std::string TargetSpec::default_output_filename() const {
  switch (format) {
  case TargetFormat::PE: return "a.exe";
  case TargetFormat::ELF: return "a.out";
  case TargetFormat::IntelHex: return "a.hex";
  case TargetFormat::AssemblyOnly: return "a.s";
  case TargetFormat::C_Source: return "a.transpiled.c";
  default: return "a.out";
  }
}

std::string TargetSpec::default_extension() const {
  switch (format) {
  case TargetFormat::PE: return ".exe";
  case TargetFormat::ELF: return "";
  case TargetFormat::IntelHex: return ".hex";
  case TargetFormat::AssemblyOnly: return ".s";
  case TargetFormat::C_Source: return ".c";
  default: return "";
  }
}

TargetSpec TargetSpec::parse(const std::string &str) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  // 1. C Transpiler
  if (lower == "c" || lower == "c_transpiler" || lower == "c-none" || lower == "transpiler") {
    return TargetSpec(TargetArch::C_Transpiler, TargetOS::None, TargetFormat::C_Source, TargetABI::None, "c-none");
  }

  // 2. AVR Bare-Metal Microcontroller
  if (lower == "avr" || lower == "arduino" || lower == "atmega328p" ||
      lower == "avr-none" || lower == "avr-none-elf" || lower == "avr-baremetal") {
    return TargetSpec(TargetArch::AVR, TargetOS::BareMetal, TargetFormat::IntelHex, TargetABI::AVR_Default, "avr-none");
  }

  // 3. ARM64 (Linux / AArch64)
  if (lower == "arm64-linux" || lower == "aarch64-linux" || lower == "aarch64-linux-gnu" ||
      lower == "arm64-linux-gnu" || lower == "arm64-elf" || lower == "aarch64-elf") {
    return TargetSpec(TargetArch::ARM64, TargetOS::Linux, TargetFormat::ELF, TargetABI::AAPCS64, "arm64-linux");
  }
  if (lower == "arm64-none" || lower == "aarch64-none") {
    return TargetSpec(TargetArch::ARM64, TargetOS::BareMetal, TargetFormat::AssemblyOnly, TargetABI::AAPCS64, "arm64-none");
  }
  if (lower == "arm64" || lower == "aarch64" || lower == "rpi" || lower == "raspberrypi" || lower == "arm") {
    return TargetSpec(TargetArch::ARM64, TargetOS::Linux, TargetFormat::ELF, TargetABI::AAPCS64, "arm64-linux");
  }

  // 4. x86_64 Linux ELF
  if (lower == "x86_64-linux" || lower == "x86-64-linux" || lower == "linux-x86_64" ||
      lower == "linux" || lower == "elf64" || lower == "elf" || lower == "x86_64-elf" ||
      lower == "x86_64-unknown-linux-gnu" || lower == "amd64-linux") {
    return TargetSpec(TargetArch::X86_64, TargetOS::Linux, TargetFormat::ELF, TargetABI::SysV_AMD64, "x86_64-linux");
  }

  // 5. x86_64 Windows PE
  if (lower == "x86_64-windows" || lower == "x86_64-win32" || lower == "x86_64-pe" ||
      lower == "windows" || lower == "win64" || lower == "x86_64-pc-windows-msvc" ||
      lower == "x86_64-w64-mingw32" || lower == "amd64-windows") {
    return TargetSpec(TargetArch::X86_64, TargetOS::Windows, TargetFormat::PE, TargetABI::Win64, "x86_64-windows");
  }

  // 6. Generic x86_64 (defaults to host platform)
  if (lower == "x86_64" || lower == "x86-64" || lower == "amd64" || lower == "x64" || lower == "x86") {
#ifdef _WIN32
    return TargetSpec(TargetArch::X86_64, TargetOS::Windows, TargetFormat::PE, TargetABI::Win64, "x86_64-windows");
#else
    return TargetSpec(TargetArch::X86_64, TargetOS::Linux, TargetFormat::ELF, TargetABI::SysV_AMD64, "x86_64-linux");
#endif
  }

  // Unknown target
  throw std::runtime_error("Unknown target '" + str +
                           "'. Run with --help to see supported target architectures and operating systems.");
}

void TargetSpec::print_supported_targets(std::ostream &out) {
  out << "Supported Target Architectures & Operating Systems (-t, --target <spec>):\n"
      << "  Target Spec        Arch       OS          Format     ABI          Description\n"
      << "  -----------------  ---------  ----------  ---------  -----------  ------------------------------------------\n"
      << "  x86_64-windows     x86_64     Windows     PE (.exe)  Win64        Windows 64-bit PE/COFF binary\n"
      << "  x86_64-linux       x86_64     Linux       ELF        SysV AMD64   Linux 64-bit static ELF executable\n"
      << "  arm64-linux        arm64      Linux       ELF        AAPCS64      Linux 64-bit ARMv8-A (Raspberry Pi, etc.)\n"
      << "  avr-none           avr        Bare-Metal  Intel HEX  AVR Default  Arduino Uno / ATmega328P flash firmware\n"
      << "  c-none             c          None        C Source   None         Portable ANSI C / C99 transpilation\n\n"
      << "Target Format Syntax:\n"
      << "  -t <arch>[-<os>]\n\n"
      << "Common Target Aliases:\n"
      << "  x86_64, amd64                  Host default (x86_64-windows on Windows, x86_64-linux on Linux)\n"
      << "  linux, elf64                   x86_64-linux\n"
      << "  arm64, aarch64, rpi            arm64-linux\n"
      << "  avr, arduino, atmega328p       avr-none\n"
      << "  c, c_transpiler                c-none\n";
}
