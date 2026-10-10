#include "codegen.hpp"
#include "codegen_avr.hpp"
#include "codegen_c.hpp"
#include "codegen_x86_64.hpp"
#include <algorithm>
#include <stdexcept>

std::unique_ptr<ICodeGenerator> CodeGeneratorFactory::create(TargetArch arch) {
  switch (arch) {
  case TargetArch::X86_64:
    return std::make_unique<X86_64CodeGenerator>();
  case TargetArch::AVR:
    return std::make_unique<AVRCodeGenerator>();
  case TargetArch::C_Transpiler:
    return std::make_unique<CCodeGenerator>();
  default:
    throw std::runtime_error("Unsupported target architecture");
  }
}

std::unique_ptr<ICodeGenerator>
CodeGeneratorFactory::create(const std::string &arch_name) {
  std::string lower = arch_name;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "x86" || lower == "x86_64" || lower == "x86-64" ||
      lower == "amd64") {
    return create(TargetArch::X86_64);
  }
  if (lower == "avr" || lower == "arduino" || lower == "atmega328p") {
    return create(TargetArch::AVR);
  }
  if (lower == "c" || lower == "c_transpiler") {
    return create(TargetArch::C_Transpiler);
  }

  throw std::runtime_error("Unknown target architecture: " + arch_name +
                           " (supported: x86_64, avr, c)");
}
