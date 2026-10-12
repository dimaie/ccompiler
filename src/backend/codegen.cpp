#include "codegen.hpp"
#include "arm64/codegen_arm64.hpp"
#include "avr/codegen_avr.hpp"
#include "c/codegen_c.hpp"
#include "x86_64/codegen_x86_64.hpp"
#include <algorithm>
#include <stdexcept>

std::unique_ptr<ICodeGenerator> CodeGeneratorFactory::create(TargetArch arch, const CodeGenOptions &options) {
  std::unique_ptr<ICodeGenerator> gen;
  switch (arch) {
  case TargetArch::X86_64:
    gen = std::make_unique<X86_64CodeGenerator>();
    break;
  case TargetArch::AVR:
    gen = std::make_unique<AVRCodeGenerator>();
    break;
  case TargetArch::ARM64:
    gen = std::make_unique<ARM64CodeGenerator>();
    break;
  case TargetArch::C_Transpiler:
    gen = std::make_unique<CCodeGenerator>();
    break;
  default:
    throw std::runtime_error("Unsupported target architecture");
  }
  gen->set_options(options);
  if (gen->get_effective_local_var_storage() == LocalVarStorage::Memory && arch != TargetArch::AVR) {
    throw UnsupportedFeatureException("local variable storage 'memory'", gen->get_arch_name());
  }
  return gen;
}

std::unique_ptr<ICodeGenerator>
CodeGeneratorFactory::create(const TargetSpec &spec, const CodeGenOptions &options) {
  CodeGenOptions opts = options;
  opts.target_spec = spec;
  return create(spec.arch, opts);
}

std::unique_ptr<ICodeGenerator>
CodeGeneratorFactory::create(const std::string &arch_name, const CodeGenOptions &options) {
  TargetSpec spec = TargetSpec::parse(arch_name);
  return create(spec, options);
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

void ICodeGenerator::visit(const ArrayAccessNode &node) {
  unsupported(node, "array access");
}

void ICodeGenerator::visit(const InitListNode &node) {
  unsupported(node, "initializer list");
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

void ICodeGenerator::visit(const StringLitNode &node) {
  unsupported(node, "string literal");
}

void ICodeGenerator::visit(const IdentifierNode &node) {
  unsupported(node, "identifier");
}

void ICodeGenerator::visit(const StructDefNode &node) {
  unsupported(node, node.is_union ? "union definition" : "struct definition");
}

void ICodeGenerator::visit(const MemberAccessNode &node) {
  unsupported(node, node.is_arrow ? "arrow member access" : "dot member access");
}

void ICodeGenerator::visit(const AsmStmtNode &node) {
  unsupported(node, "inline assembly");
}

void ICodeGenerator::visit(const TernaryOpNode &node) {
  unsupported(node, "ternary operator");
}

