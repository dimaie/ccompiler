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

  struct MemberInfo {
    std::string name;
    DataType type{DataType::Int};
    int pointer_level{0};
    int offset{0};
    int size{0};
    bool is_array{false};
    int array_size{0};
    std::string struct_name;
    bool is_union{false};
  };

  struct StructInfo {
    std::string name;
    bool is_union{false};
    int total_size{0};
    std::vector<MemberInfo> members;
    std::unordered_map<std::string, MemberInfo> member_map;
  };

private:
  std::ostream *out_{nullptr};
  std::unordered_map<std::string, int> var_offsets_;
  std::unordered_map<std::string, DataType> var_types_;
  std::unordered_map<std::string, int> var_pointer_level_;
  std::unordered_map<std::string, bool> is_array_;
  std::unordered_map<std::string, int> array_sizes_;
  std::unordered_map<std::string, bool> is_func_ptr_;
  std::unordered_map<std::string, std::string> var_struct_name_;
  std::unordered_map<std::string, StructInfo> struct_defs_;
  int current_offset_{0};
  int label_counter_{0};
  std::string ret_label_{".Lmain_ret"};
  std::vector<std::string> break_stack_;
  std::vector<std::string> continue_stack_;
  std::vector<std::pair<std::string, std::string>> string_literals_;

  std::string new_label(const std::string &prefix);
  void emit_line(const std::string &inst);
  void emit_raw(const std::string &line);
  void emit_string_literal(const std::string &str);
  int get_var_offset(const std::string &name);
  DataType get_var_type(const std::string &name) const;
  int get_var_pointer_level(const std::string &name) const;
  bool is_func_ptr(const std::string &name) const;
  DataType get_expr_type(const ASTNode *expr, int &pointer_level) const;
  std::string get_expr_struct_type(const ASTNode *expr) const;
  int get_type_size(DataType type, const std::string &st_name = "") const;
};

#endif // CODEGEN_X86_64_HPP
