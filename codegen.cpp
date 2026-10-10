#include "codegen.hpp"
#include "codegen_arm64.hpp"
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
  case TargetArch::ARM64:
    return std::make_unique<ARM64CodeGenerator>();
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
  if (lower == "arm" || lower == "arm64" || lower == "aarch64" ||
      lower == "rpi" || lower == "raspberrypi") {
    return create(TargetArch::ARM64);
  }
  if (lower == "c" || lower == "c_transpiler") {
    return create(TargetArch::C_Transpiler);
  }

  throw std::runtime_error("Unknown target architecture: " + arch_name +
                           " (supported: x86_64, avr, arm64, c)");
}

void ICodeGenerator::visit(const ProgramNode &node) {
  unsupported(node, "program");
}

void ICodeGenerator::visit(const CompoundNode &node) {
  unsupported(node, "compound statement");
}

void ICodeGenerator::visit(const VarDeclNode &node) {
  unsupported(node, "variable declaration");
}

void ICodeGenerator::visit(const FuncDefNode &node) {
  unsupported(node, "function definition");
}

void ICodeGenerator::visit(const CallNode &node) {
  unsupported(node, "function call");
}

void ICodeGenerator::visit(const IfNode &node) {
  unsupported(node, "if statement");
}

void ICodeGenerator::visit(const ForNode &node) {
  unsupported(node, "for loop");
}

void ICodeGenerator::visit(const WhileNode &node) {
  unsupported(node, "while loop");
}

void ICodeGenerator::visit(const DoWhileNode &node) {
  unsupported(node, "do..while loop");
}

void ICodeGenerator::visit(const SwitchNode &node) {
  unsupported(node, "switch statement");
}

void ICodeGenerator::visit(const CaseNode &node) {
  unsupported(node, "case label");
}

void ICodeGenerator::visit(const BreakNode &node) {
  unsupported(node, "break statement");
}

void ICodeGenerator::visit(const ContinueNode &node) {
  unsupported(node, "continue statement");
}

void ICodeGenerator::visit(const GotoNode &node) {
  unsupported(node, "goto statement");
}

void ICodeGenerator::visit(const LabelNode &node) {
  unsupported(node, "label statement");
}

void ICodeGenerator::visit(const ReturnNode &node) {
  unsupported(node, "return statement");
}

void ICodeGenerator::visit(const AssignNode &node) {
  unsupported(node, "assignment");
}

void ICodeGenerator::visit(const BinaryOpNode &node) {
  unsupported(node, "binary operator");
}

void ICodeGenerator::visit(const UnaryOpNode &node) {
  unsupported(node, "unary operator");
}

void ICodeGenerator::visit(const IntLitNode &node) {
  unsupported(node, "integer literal");
}

void ICodeGenerator::visit(const IdentifierNode &node) {
  unsupported(node, "identifier");
}

