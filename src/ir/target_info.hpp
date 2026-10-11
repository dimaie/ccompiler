#ifndef TARGET_INFO_HPP
#define TARGET_INFO_HPP

#include "codegen.hpp"
#include <string>

struct TargetInfo {
  TargetSpec spec;
  TargetArch arch{TargetArch::X86_64};
  std::string name{"x86_64"};
  LocalVarStorage local_var_storage{LocalVarStorage::Auto};

  int char_size{1};
  int short_size{2};
  int int_size{4};
  int long_size{8};
  int pointer_size{8};

  bool is_64bit{true};
  bool is_little_endian{true};

  LocalVarStorage get_effective_local_var_storage() const {
    if (local_var_storage == LocalVarStorage::Auto) {
      return (arch == TargetArch::AVR) ? LocalVarStorage::Memory : LocalVarStorage::Stack;
    }
    return local_var_storage;
  }

  bool is_memory_mode() const {
    return get_effective_local_var_storage() == LocalVarStorage::Memory;
  }

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

  static TargetInfo from_spec(const TargetSpec &spec, LocalVarStorage storage = LocalVarStorage::Auto) {
    TargetInfo info = get(spec.arch);
    info.spec = spec;
    info.name = spec.to_string();
    info.local_var_storage = storage;
    return info;
  }

  static TargetInfo from_string(const std::string &arch_name, LocalVarStorage storage = LocalVarStorage::Auto) {
    try {
      TargetSpec spec = TargetSpec::parse(arch_name);
      return from_spec(spec, storage);
    } catch (...) {
      TargetInfo info = get(TargetArch::X86_64);
      info.local_var_storage = storage;
      return info;
    }
  }
};

#endif // TARGET_INFO_HPP
