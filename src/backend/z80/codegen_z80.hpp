#ifndef CODEGEN_Z80_HPP
#define CODEGEN_Z80_HPP

#include "codegen.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

class Z80CodeGenerator : public ICodeGenerator {
public:
  TargetArch get_arch() const override { return TargetArch::Z80; }
  const char *get_arch_name() const override { return "z80"; }
  void generate(const ProgramNode &program, std::ostream &out) override;

  void visit(const ProgramNode &node) override;
  void visit(const CompoundNode &node) override;
  void visit(const VarDeclNode &node) override;
  void visit(const FuncDefNode &node) override;
  void visit(const CallNode &node) override;
  void visit(const ArrayAccessNode &node) override;
  void visit(const IfNode &node) override;
  void visit(const ForNode &node) override;
  void visit(const WhileNode &node) override;
  void visit(const DoWhileNode &node) override;
  void visit(const SwitchNode &node) override;
  void visit(const BreakNode &node) override;
  void visit(const ContinueNode &node) override;
  void visit(const GotoNode &node) override;
  void visit(const LabelNode &node) override;
  void visit(const ReturnNode &node) override;
  void visit(const AssignNode &node) override;
  void visit(const BinaryOpNode &node) override;
  void visit(const UnaryOpNode &node) override;
  void visit(const IntLitNode &node) override;
  void visit(const StringLitNode &node) override;
  void visit(const IdentifierNode &node) override;
  void visit(const AsmStmtNode &node) override;
  void visit(const TernaryOpNode &node) override;

private:
  struct Type {
    DataType base{DataType::Int};
    int pointers{0};
    bool array{false};
  };
  struct Variable {
    Type type;
    int offset{0};
    std::string symbol;
  };
  using Scope = std::unordered_map<std::string, Variable>;
  std::ostream *out_{nullptr};
  Scope globals_;
  std::vector<Scope> scopes_;
  std::unordered_map<std::string, const FuncDefNode *> functions_;
  std::vector<std::pair<std::string, std::string>> strings_;
  std::vector<std::string> breaks_, continues_;
  std::string function_, return_label_;
  Type return_type_;
  int frame_size_{0};
  int label_counter_{0};

  void emit(const std::string &text);
  void label(const std::string &name);
  std::string new_label(const std::string &prefix);
  void test_zero();
  void normalize(Type type);
  void negate();
  uint16_t literal_constant(const ASTNode &node) const;
  int size(Type type) const;
  Type checked_type(DataType base, int pointers, const ASTNode &node) const;
  Type expression_type(const ASTNode &node) const;
  Type pointee(Type type, const ASTNode &node) const;
  const Variable &lookup(const IdentifierNode &node) const;
  void address(const Variable &variable);
  void address(const ASTNode &node);
  void load(Type type);
  void store(Type type);
  void branch_zero(const ASTNode &expr, const std::string &destination);
  void emit_global(const VarDeclNode &node);
};

#endif // CODEGEN_Z80_HPP
