#ifndef IR_BUILDER_HPP
#define IR_BUILDER_HPP

#include "ast.hpp"
#include "ir.hpp"
#include "target_info.hpp"
#include <unordered_map>
#include <vector>

class IRBuilder : public ASTVisitor {
public:
  explicit IRBuilder(TargetInfo target_info = TargetInfo::get(TargetArch::X86_64))
      : target_info_(std::move(target_info)) {}

  IRModule build(const ProgramNode &program);

  // ASTVisitor interface
  void visit(const ProgramNode &node) override;
  void visit(const CompoundNode &node) override;
  void visit(const VarDeclNode &node) override;
  void visit(const FuncDefNode &node) override;
  void visit(const CallNode &node) override;
  void visit(const ArrayAccessNode &node) override;
  void visit(const InitListNode &node) override;
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
  void visit(const StringLitNode &node) override;
  void visit(const IdentifierNode &node) override;
  void visit(const StructDefNode &node) override;
  void visit(const MemberAccessNode &node) override;
  void visit(const AsmStmtNode &node) override;
  void visit(const TernaryOpNode &node) override;

private:
  TargetInfo target_info_;
  IRModule module_;
  IRFunction *current_func_{nullptr};

  int next_var_id_{0};
  int next_label_id_{0};

  IROperand last_expr_operand_{IROperand::none()};

  struct VarInfo {
    IROperand op;
    bool is_memory{false};
    bool is_array{false};
    int size{0};
  };

  // Variable tracking: symbol name -> VarInfo
  std::unordered_map<std::string, VarInfo> var_table_;

  // Break / continue label stacks
  std::vector<std::string> break_labels_;
  std::vector<std::string> continue_labels_;

  // Helpers
  IROperand new_var();
  std::string new_label(const std::string &prefix = "L");
  IROperand lower_expression(const ASTNode &node);
  void emit_instruction(IRInstruction inst);
};

#endif // IR_BUILDER_HPP
