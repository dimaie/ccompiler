#include "assembler_linker.hpp"
#include "internal_assembler.hpp"
#include "internal_pe_linker.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

AssemblerLinkerResult X86_64AssemblerLinker::link_modules(
    const std::vector<AssembledModule> &modules,
    const std::string &output_filepath,
    TargetFileType target_file_type) {
  AssemblerLinkerResult result;
  std::string error_msg;
  bool success = InternalPELinker::link(modules, output_filepath, error_msg, config_, target_file_type);
  if (!success) {
    result.success = false;
    result.exit_code = 1;
    result.message = "Internal linker error: " + error_msg;
    return result;
  }

  result.success = true;
  result.exit_code = 0;
  result.output_path = output_filepath;
  result.message = "Successfully assembled and linked executable using embedded toolchain.";
  return result;
}

AssemblerLinkerResult X86_64AssemblerLinker::assemble_and_link_multiple(
    const std::vector<std::string> &assembly_codes,
    const std::string &output_filepath,
    TargetFileType target_file_type) {
  try {
    X86_64InternalAssembler internal_assembler;
    std::vector<AssembledModule> modules;
    modules.reserve(assembly_codes.size());
    for (const auto &asm_code : assembly_codes) {
      modules.push_back(internal_assembler.assemble(asm_code));
    }
    return link_modules(modules, output_filepath, target_file_type);
  } catch (const std::exception &e) {
    AssemblerLinkerResult result;
    result.success = false;
    result.exit_code = 1;
    result.message = std::string("Internal assembler error: ") + e.what();
    return result;
  }
}

AssemblerLinkerResult X86_64AssemblerLinker::assemble_and_link(
    const std::string &assembly_code,
    const std::string &output_filepath) {
  return assemble_and_link_multiple({assembly_code}, output_filepath, TargetFileType::Auto);
}

AssemblerLinkerResult StubAssemblerLinker::link_modules(
    const std::vector<AssembledModule> &/*modules*/,
    const std::string &/*output_filepath*/,
    TargetFileType /*target_file_type*/) {
  AssemblerLinkerResult result;
  result.success = false;
  result.exit_code = 1;
  result.message = "Notice: Assembler and linker features are not yet available for target '" +
                   arch_name_ + "'. Compilation stopped at assembly phase.";
  return result;
}

AssemblerLinkerResult StubAssemblerLinker::assemble_and_link_multiple(
    const std::vector<std::string> &assembly_codes,
    const std::string &output_filepath,
    TargetFileType /*target_file_type*/) {
  AssemblerLinkerResult result;
  result.success = false;
  result.exit_code = 1;
  result.message = "Notice: Assembler and linker features are not yet available for target '" +
                   arch_name_ + "'. Compilation stopped at assembly phase.";

  if (!assembly_codes.empty()) {
    std::ofstream out(output_filepath);
    if (out.is_open()) {
      for (const auto &c : assembly_codes) {
        out << c << "\n";
      }
      result.output_path = output_filepath;
    }
  }

  return result;
}

AssemblerLinkerResult StubAssemblerLinker::assemble_and_link(
    const std::string &assembly_code,
    const std::string &output_filepath) {
  return assemble_and_link_multiple({assembly_code}, output_filepath, TargetFileType::Auto);
}

std::unique_ptr<IAssemblerLinker>
AssemblerLinkerFactory::create(TargetArch arch) {
  switch (arch) {
  case TargetArch::X86_64:
    return std::make_unique<X86_64AssemblerLinker>();
  case TargetArch::ARM64:
    return std::make_unique<StubAssemblerLinker>(TargetArch::ARM64, "arm64");
  case TargetArch::AVR:
    return std::make_unique<StubAssemblerLinker>(TargetArch::AVR, "avr");
  case TargetArch::C_Transpiler:
    return std::make_unique<StubAssemblerLinker>(TargetArch::C_Transpiler, "c");
  default:
    return std::make_unique<StubAssemblerLinker>(arch, "unknown");
  }
}

std::unique_ptr<IAssemblerLinker>
AssemblerLinkerFactory::create(const std::string &arch_name) {
  std::string lower = arch_name;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "x86_64" || lower == "x86" || lower == "x64" ||
      lower == "amd64") {
    return create(TargetArch::X86_64);
  }
  if (lower == "arm" || lower == "arm64" || lower == "aarch64" ||
      lower == "rpi" || lower == "raspberrypi") {
    return create(TargetArch::ARM64);
  }
  if (lower == "avr" || lower == "arduino" || lower == "atmega328p") {
    return create(TargetArch::AVR);
  }
  if (lower == "c" || lower == "c_transpiler") {
    return create(TargetArch::C_Transpiler);
  }

  return std::make_unique<StubAssemblerLinker>(TargetArch::X86_64, arch_name);
}
