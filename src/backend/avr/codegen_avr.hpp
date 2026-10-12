#ifndef CODEGEN_AVR_HPP
#define CODEGEN_AVR_HPP

#include "codegen.hpp"
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Code generator for 8-bit AVR RISC architecture (ATmega328P / Arduino UNO).
 * Follows the standard 16-bit int avr-gcc ABI (r24:r25 primary accumulator).
 * Supports both stack-allocated local variables (re-entrant) and static memory allocation (.bss, fast).
 */
class AVRCodeGenerator : public ICodeGenerator {
public:
  AVRCodeGenerator() = default;

  TargetArch get_arch() const override { return TargetArch::AVR; }
  const char *get_arch_name() const override { return "avr"; }

  bool is_memory_mode() const {
    return get_effective_local_var_storage() == LocalVarStorage::Memory;
  }

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
  void visit(const IdentifierNode &node) override;
  void visit(const StructDefNode &node) override;
  void visit(const MemberAccessNode &node) override;
  void visit(const AsmStmtNode &node) override;
  void visit(const TernaryOpNode &node) override;

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

  struct MemoryVarDef {
    std::string symbol;
    int size{2};
  };

private:
  std::ostream *out_{nullptr};
  std::unordered_map<std::string, int> var_offsets_;
  std::unordered_map<std::string, bool> is_array_;
  std::unordered_map<std::string, int> array_sizes_;
  std::unordered_map<std::string, bool> is_func_ptr_;
  std::unordered_map<std::string, std::string> var_struct_name_;
  std::unordered_map<std::string, StructInfo> struct_defs_;
  int current_offset_{0};
  int label_counter_{0};
  std::string ret_label_{".Lmain_avr_ret"};
  std::vector<std::string> break_stack_;
  std::vector<std::string> continue_stack_;

  std::string current_func_name_{"main"};
  std::unordered_map<std::string, std::string> var_symbols_;
  std::vector<MemoryVarDef> memory_vars_;
  std::unordered_set<std::string> defined_symbols_;
  int var_sym_counter_{0};
  int last_emitted_line_{0};
  int last_emitted_file_id_{0};

  std::string new_label(const std::string &prefix);
  void emit_line(const std::string &inst);
  void emit_raw(const std::string &line);
  void emit_loc(int line, int file_id = 1);
  int get_var_offset(const std::string &name);
  std::string alloc_memory_var(const std::string &name, int size);
  std::string get_var_symbol(const std::string &name);
  std::string get_expr_struct_type(const ASTNode *expr) const;
  int get_type_size(DataType type, const std::string &st_name = "") const;
};

#endif // CODEGEN_AVR_HPP
