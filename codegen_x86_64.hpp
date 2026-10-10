#ifndef CODEGEN_X86_64_HPP
#define CODEGEN_X86_64_HPP

#include "codegen.hpp"
#include <ostream>
#include <string>
#include <unordered_map>

class X86_64CodeGenerator : public ICodeGenerator {
public:
  X86_64CodeGenerator() = default;

  TargetArch get_arch() const override { return TargetArch::X86_64; }
  const char *get_arch_name() const override { return "x86_64"; }

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
  void visit(const ContinueNode &node) override;
  void visit(const GotoNode &node) override;
  void visit(const LabelNode &node) override;
  void visit(const ReturnNode &node) override;
  void visit(const AssignNode &node) override;
  void visit(const BinaryOpNode &node) override;
  void visit(const UnaryOpNode &node) override;
  void visit(const IntLitNode &node) override;
  void visit(const IdentifierNode &node) override;

private:
  std::ostream *out_{nullptr};
  std::unordered_map<std::string, int> var_offsets_;
  int current_offset_{0};
  int label_counter_{0};
  std::string ret_label_{".Lmain_ret"};
  std::vector<std::string> break_stack_;
  std::vector<std::string> continue_stack_;

  std::string new_label(const std::string &prefix);
  void emit_line(const std::string &inst);
  void emit_raw(const std::string &line);
  int get_var_offset(const std::string &name);
};

#endif // CODEGEN_X86_64_HPP
