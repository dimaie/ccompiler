#include "assembler_linker.hpp"
#include "internal_assembler.hpp"
#include "internal_pe_linker.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

AssemblerLinkerResult X86_64AssemblerLinker::assemble_and_link(
    const std::string &assembly_code,
    const std::string &output_filepath) {
  AssemblerLinkerResult result;

  try {
    X86_64InternalAssembler internal_assembler;
    AssembledModule module = internal_assembler.assemble(assembly_code);

    std::string error_msg;
    bool success = InternalPELinker::link(std::move(module), output_filepath, error_msg);
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
  } catch (const std::exception &e) {
    result.success = false;
    result.exit_code = 1;
    result.message = std::string("Internal assembler error: ") + e.what();
    return result;
  }
}

AssemblerLinkerResult StubAssemblerLinker::assemble_and_link(
    const std::string &assembly_code,
    const std::string &output_filepath) {
  AssemblerLinkerResult result;
  result.success = false;
  result.exit_code = 1;
  result.message = "Notice: Assembler and linker features are not yet available for target '" +
                   arch_name_ + "'. Compilation stopped at assembly phase.";

  // Save the generated assembly code to output_filepath so the phase completes
  std::ofstream out(output_filepath);
  if (out.is_open()) {
    out << assembly_code;
    result.output_path = output_filepath;
  }

  return result;
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
