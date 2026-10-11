#ifndef IR_HPP
#define IR_HPP

#include "ast.hpp"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

enum class IROpKind {
  Nop,
  // Arithmetic / Bitwise
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Neg,
  Not,
  And,
  Or,
  Xor,
  Shl,
  Shr,
  // Comparison
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  // Memory & Assignment
  Copy,
  Load,
  Store,
  Alloca,
  GetElementPtr,
  // Control Flow
  Label,
  Jump,
  BranchIf,
  Call,
  Return
};

const char *ir_op_str(IROpKind op);

enum class IROperandKind {
  None,
  Var,     // Virtual register: %v0, %v1, ...
  Imm,     // Immediate integer: $42
  Label,   // Control flow label: .L1
  Global   // Global symbol: @sym
};

struct IROperand {
  IROperandKind kind{IROperandKind::None};
  int var_id{0};
  int64_t imm_val{0};
  std::string name;

  static IROperand none() { return IROperand{}; }
  static IROperand var(int id) {
    IROperand op;
    op.kind = IROperandKind::Var;
    op.var_id = id;
    return op;
  }
  static IROperand imm(int64_t val) {
    IROperand op;
    op.kind = IROperandKind::Imm;
    op.imm_val = val;
    return op;
  }
  static IROperand label(std::string name) {
    IROperand op;
    op.kind = IROperandKind::Label;
    op.name = std::move(name);
    return op;
  }
  static IROperand global(std::string name) {
    IROperand op;
    op.kind = IROperandKind::Global;
    op.name = std::move(name);
    return op;
  }

  std::string to_string() const;
};

struct IRInstruction {
  IROpKind op{IROpKind::Nop};
  IROperand dst;
  IROperand src1;
  IROperand src2;
  std::vector<IROperand> args;
  std::string comment;

  void print(std::ostream &out) const;
};

struct IRFunction {
  std::string name;
  bool is_static{false};
  DataType return_type{DataType::Int};
  std::vector<IROperand> params;
  std::vector<IRInstruction> instructions;

  void print(std::ostream &out) const;
};

struct IRModule {
  std::vector<IRFunction> functions;
  std::vector<std::pair<std::string, std::string>> string_literals;
  std::vector<std::pair<std::string, int>> global_vars;

  void print(std::ostream &out) const;
};

#endif // IR_HPP
