#ifndef TARGET_INFO_HPP
#define TARGET_INFO_HPP

#include "codegen.hpp"
#include <string>

struct TargetInfo {
  TargetArch arch{TargetArch::X86_64};
  std::string name{"x86_64"};

  int char_size{1};
  int short_size{2};
  int int_size{4};
  int long_size{8};
  int pointer_size{8};

  bool is_64bit{true};
  bool is_little_endian{true};

  int get_type_size(DataType type) const {
    switch (type) {
    case DataType::Char: return char_size;
    case DataType::Short: return short_size;
    case DataType::Int: return int_size;
    case DataType::Long: return long_size;
    case DataType::LongLong: return 8;
    case DataType::Void: return 0;
    default: return int_size;
    }
  }

  static TargetInfo get(TargetArch arch) {
    TargetInfo info;
    info.arch = arch;
    switch (arch) {
    case TargetArch::X86_64:
      info.name = "x86_64";
      info.int_size = 4;
      info.long_size = 8;
      info.pointer_size = 8;
      info.is_64bit = true;
      break;
    case TargetArch::ARM64:
      info.name = "arm64";
      info.int_size = 4;
      info.long_size = 8;
      info.pointer_size = 8;
      info.is_64bit = true;
      break;
    case TargetArch::AVR:
      info.name = "avr";
      info.int_size = 2;
      info.long_size = 4;
      info.pointer_size = 2;
      info.is_64bit = false;
      break;
    case TargetArch::C_Transpiler:
      info.name = "c";
      info.int_size = 4;
      info.long_size = 8;
      info.pointer_size = 8;
      info.is_64bit = true;
      break;
    }
    return info;
  }

  static TargetInfo from_string(const std::string &arch_name) {
    std::string lower = arch_name;
    for (char &c : lower) {
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    if (lower == "x86" || lower == "x86_64" || lower == "x86-64" || lower == "amd64") {
      return get(TargetArch::X86_64);
    }
    if (lower == "avr" || lower == "arduino" || lower == "atmega328p") {
      return get(TargetArch::AVR);
    }
    if (lower == "arm" || lower == "arm64" || lower == "aarch64" || lower == "rpi" || lower == "raspberrypi") {
      return get(TargetArch::ARM64);
    }
    if (lower == "c" || lower == "c_transpiler") {
      return get(TargetArch::C_Transpiler);
    }
    return get(TargetArch::X86_64);
  }
};

#endif // TARGET_INFO_HPP
