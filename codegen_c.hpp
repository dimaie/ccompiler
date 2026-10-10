#ifndef CODEGEN_C_HPP
#define CODEGEN_C_HPP

#include "codegen.hpp"
#include <ostream>
#include <string>

/**
 * @brief Reference C transpiler backend that outputs clean C code from AST.
 */
class CCodeGenerator : public ICodeGenerator {
public:
  CCodeGenerator() = default;

  TargetArch get_arch() const override { return TargetArch::C_Transpiler; }
  const char *get_arch_name() const override { return "c"; }

  void generate(const ProgramNode &program, std::ostream &out) override;

  void visit(const ProgramNode &node) override;
  void visit(const CompoundNode &node) override;
  void visit(const VarDeclNode &node) override;
  void visit(const IfNode &node) override;
  void visit(const ForNode &node) override;
  void visit(const WhileNode &node) override;
  void visit(const DoWhileNode &node) override;
  void visit(const SwitchNode &node) override;
  void visit(const CaseNode &node) override;
  void visit(const BreakNode &node) override;
  void visit(const ReturnNode &node) override;
  void visit(const AssignNode &node) override;
  void visit(const BinaryOpNode &node) override;
  void visit(const UnaryOpNode &node) override;
  void visit(const IntLitNode &node) override;
  void visit(const IdentifierNode &node) override;

private:
  std::ostream *out_{nullptr};
  int indent_{0};

  void print_indent();
};

#endif // CODEGEN_C_HPP
