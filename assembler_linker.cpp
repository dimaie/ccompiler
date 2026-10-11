#include "assembler_linker.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

int run_command(const std::string &cmd, std::string &output) {
#ifdef _WIN32
  FILE *pipe = _popen(cmd.c_str(), "r");
#else
  FILE *pipe = popen(cmd.c_str(), "r");
#endif
  if (!pipe) {
    return -1;
  }
  char buffer[512];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }
#ifdef _WIN32
  return _pclose(pipe);
#else
  return pclose(pipe);
#endif
}

bool command_exists(const std::string &cmd) {
  std::string out;
  int code = run_command(cmd + " --version 2>&1", out);
  return code == 0;
}

} // namespace

std::string X86_64AssemblerLinker::find_toolchain() const {
  const char *env_toolchain = std::getenv("C_COMPILER_TOOLCHAIN");
  if (env_toolchain && *env_toolchain) {
    return std::string(env_toolchain);
  }
  const char *env_cc = std::getenv("CC");
  if (env_cc && *env_cc) {
    return std::string(env_cc);
  }

  if (command_exists("gcc")) {
    return "gcc";
  }
  if (command_exists("clang")) {
    return "clang";
  }

  return "";
}

AssemblerLinkerResult X86_64AssemblerLinker::assemble_and_link(
    const std::string &assembly_code,
    const std::string &output_filepath) {
  AssemblerLinkerResult result;

  std::string toolchain = find_toolchain();
  if (toolchain.empty()) {
    result.success = false;
    result.message =
        "No suitable x86_64 assembler and linker toolchain (gcc or clang) "
        "found in PATH.\nPlease install GCC/Clang or set the CC environment variable.";
    return result;
  }

  std::filesystem::path temp_s;
  try {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    temp_s = std::filesystem::temp_directory_path() /
             ("cc_asm_" + std::to_string(now) + ".s");
  } catch (...) {
    temp_s = output_filepath + ".tmp.s";
  }

  {
    std::ofstream s_file(temp_s);
    if (!s_file.is_open()) {
      result.success = false;
      result.message = "Failed to create temporary assembly file: " + temp_s.string();
      return result;
    }
    s_file << assembly_code;
  }

  std::string cmd = toolchain + " \"" + temp_s.string() + "\" -o \"" +
                    output_filepath + "\" 2>&1";
  std::string cmd_output;
  int exit_code = run_command(cmd, cmd_output);

  std::error_code ec;
  std::filesystem::remove(temp_s, ec);

  result.exit_code = exit_code;
  if (exit_code == 0) {
    result.success = true;
    result.output_path = output_filepath;
    result.message = cmd_output;
  } else {
    result.success = false;
    result.message = "Assembler and linker error (exit code " +
                     std::to_string(exit_code) + "):\n" + cmd_output;
  }

  return result;
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
