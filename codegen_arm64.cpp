#include "codegen_arm64.hpp"
#include <stdexcept>

std::string ARM64CodeGenerator::new_label(const std::string &prefix) {
  return prefix + "_" + std::to_string(++label_counter_);
}

void ARM64CodeGenerator::emit_line(const std::string &inst) {
  *out_ << "    " << inst << "\n";
}

void ARM64CodeGenerator::emit_raw(const std::string &line) {
  *out_ << line << "\n";
}

int ARM64CodeGenerator::get_var_offset(const std::string &name) {
  auto it = var_offsets_.find(name);
  if (it != var_offsets_.end()) {
    return it->second;
  }
  // Local variables stored at [x29, #offset]
  current_offset_ += 8;
  var_offsets_[name] = current_offset_;
  return current_offset_;
}

void ARM64CodeGenerator::generate(const ProgramNode &program,
                                  std::ostream &out) {
  out_ = &out;
  var_offsets_.clear();
  current_offset_ = 8; // offset 0 and 8 reserved for saved x29, x30
  label_counter_ = 0;
  break_stack_.clear();

  emit_raw("// ================================================================");
  emit_raw("// ARM64 (AArch64) Assembly for Raspberry Pi (ARMv8-A)");
  emit_raw("// ABI: AAPCS64, x0 return/accumulator, x29 frame pointer");
  emit_raw("// ================================================================");
  emit_raw(".arch armv8-a");
  emit_raw(".text");
  emit_raw(".globl main");
  emit_raw(".type main, %function");
  emit_raw("main:");

  // Prologue: Allocate 64 bytes (16-byte aligned) & save frame pointer + link register
  emit_line("stp x29, x30, [sp, -64]!    // save FP and LR, alloc frame");
  emit_line("mov x29, sp                 // set frame pointer");

  program.accept(*this);

  // Epilogue: Restore frame and return
  emit_raw(ret_label_ + ":");
  emit_line("mov sp, x29                 // restore stack pointer");
  emit_line("ldp x29, x30, [sp], 64      // restore FP and LR, dealloc frame");
  emit_line("ret");
}

void ARM64CodeGenerator::visit(const ProgramNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void ARM64CodeGenerator::visit(const CompoundNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void ARM64CodeGenerator::visit(const VarDeclNode &node) {
  int offset = get_var_offset(node.name);
  if (node.init) {
    node.init->accept(*this);
    emit_line("str x0, [x29, #" + std::to_string(offset) + "]");
  }
}

void ARM64CodeGenerator::visit(const AssignNode &node) {
  if (!node.left || node.left->getType() != ASTNodeType::Identifier) {
    throw std::runtime_error("Left-hand side of assignment must be variable");
  }
  auto *ident = static_cast<const IdentifierNode *>(node.left.get());
  int offset = get_var_offset(ident->name);

  if (node.right) {
    node.right->accept(*this);
  }
  emit_line("str x0, [x29, #" + std::to_string(offset) + "]");
}

void ARM64CodeGenerator::visit(const IntLitNode &node) {
  if (node.val >= 0 && node.val <= 65535) {
    emit_line("mov x0, #" + std::to_string(node.val));
  } else {
    emit_line("ldr x0, =" + std::to_string(node.val));
  }
}

void ARM64CodeGenerator::visit(const IdentifierNode &node) {
  int offset = get_var_offset(node.name);
  emit_line("ldr x0, [x29, #" + std::to_string(offset) + "]");
}

void ARM64CodeGenerator::visit(const UnaryOpNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  switch (node.op) {
  case TOK_MINUS:
    emit_line("neg x0, x0");
    break;
  case TOK_PLUS:
    break;
  case TOK_LOGIC_NOT:
    emit_line("cmp x0, #0");
    emit_line("cset x0, eq");
    break;
  case TOK_TILDE:
    emit_line("mvn x0, x0");
    break;
  default:
    unsupported("Unary operator '" + std::string(token_op_str(node.op)) + "'", node.line);
  }
}

void ARM64CodeGenerator::visit(const BinaryOpNode &node) {
  if (node.op == TOK_LOGIC_AND) {
    std::string false_lbl = new_label(".Larm_and_false");
    std::string end_lbl = new_label(".Larm_and_end");
    if (node.left) node.left->accept(*this);
    emit_line("cbz x0, " + false_lbl);
    if (node.right) node.right->accept(*this);
    emit_line("cbz x0, " + false_lbl);
    emit_line("mov x0, #1");
    emit_line("b " + end_lbl);
    emit_raw(false_lbl + ":");
    emit_line("mov x0, #0");
    emit_raw(end_lbl + ":");
    return;
  }

  if (node.op == TOK_LOGIC_OR) {
    std::string true_lbl = new_label(".Larm_or_true");
    std::string end_lbl = new_label(".Larm_or_end");
    if (node.left) node.left->accept(*this);
    emit_line("cbnz x0, " + true_lbl);
    if (node.right) node.right->accept(*this);
    emit_line("cbnz x0, " + true_lbl);
    emit_line("mov x0, #0");
    emit_line("b " + end_lbl);
    emit_raw(true_lbl + ":");
    emit_line("mov x0, #1");
    emit_raw(end_lbl + ":");
    return;
  }

  // Evaluate left and push onto stack (16-byte aligned for SP)
  if (node.left) {
    node.left->accept(*this);
  }
  emit_line("str x0, [sp, -16]!");

  // Evaluate right (result in x0)
  if (node.right) {
    node.right->accept(*this);
  }

  // Move right to x1, restore left into x0
  emit_line("mov x1, x0");
  emit_line("ldr x0, [sp], 16");

  switch (node.op) {
  case TOK_PLUS:
    emit_line("add x0, x0, x1");
    break;
  case TOK_MINUS:
    emit_line("sub x0, x0, x1");
    break;
  case TOK_STAR:
    emit_line("mul x0, x0, x1");
    break;
  case TOK_SLASH:
    emit_line("sdiv x0, x0, x1");
    break;
  case TOK_PERCENT:
    // modulo: x0 = x0 - (x0 / x1) * x1
    emit_line("sdiv x2, x0, x1");
    emit_line("msub x0, x2, x1, x0");
    break;
  case TOK_EQUAL:
    emit_line("cmp x0, x1");
    emit_line("cset x0, eq");
    break;
  case TOK_NOT_EQUAL:
    emit_line("cmp x0, x1");
    emit_line("cset x0, ne");
    break;
  case TOK_LESS:
    emit_line("cmp x0, x1");
    emit_line("cset x0, lt");
    break;
  case TOK_LESS_EQ:
    emit_line("cmp x0, x1");
    emit_line("cset x0, le");
    break;
  case TOK_GREATER:
    emit_line("cmp x0, x1");
    emit_line("cset x0, gt");
    break;
  case TOK_GREATER_EQ:
    emit_line("cmp x0, x1");
    emit_line("cset x0, ge");
    break;
  default:
    unsupported("Binary operator '" + std::string(token_op_str(node.op)) + "'", node.line);
  }
}

void ARM64CodeGenerator::visit(const IfNode &node) {
  std::string else_lbl = new_label(".Larm_if_else");
  std::string end_lbl = new_label(".Larm_if_end");

  if (node.cond) {
    node.cond->accept(*this);
  }
  emit_line("cbz x0, " + (node.else_branch ? else_lbl : end_lbl));

  if (node.then_branch) {
    node.then_branch->accept(*this);
  }

  if (node.else_branch) {
    emit_line("b " + end_lbl);
    emit_raw(else_lbl + ":");
    node.else_branch->accept(*this);
  }
  emit_raw(end_lbl + ":");
}

void ARM64CodeGenerator::visit(const ForNode &node) {
  std::string cond_lbl = new_label(".Larm_for_cond");
  std::string step_lbl = new_label(".Larm_for_step");
  std::string end_lbl = new_label(".Larm_for_end");

  if (node.init) {
    node.init->accept(*this);
  }
  emit_raw(cond_lbl + ":");

  if (node.cond) {
    node.cond->accept(*this);
    emit_line("cbz x0, " + end_lbl);
  }

  break_stack_.push_back(end_lbl);
  if (node.body) {
    node.body->accept(*this);
  }
  break_stack_.pop_back();

  emit_raw(step_lbl + ":");
  if (node.step) {
    node.step->accept(*this);
  }
  emit_line("b " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void ARM64CodeGenerator::visit(const WhileNode &node) {
  std::string cond_lbl = new_label(".Larm_while_cond");
  std::string end_lbl = new_label(".Larm_while_end");

  emit_raw(cond_lbl + ":");
  if (node.cond) {
    node.cond->accept(*this);
    emit_line("cbz x0, " + end_lbl);
  }

  break_stack_.push_back(end_lbl);
  if (node.body) {
    node.body->accept(*this);
  }
  break_stack_.pop_back();

  emit_line("b " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void ARM64CodeGenerator::visit(const DoWhileNode &node) {
  std::string body_lbl = new_label(".Larm_dowhile_body");
  std::string end_lbl = new_label(".Larm_dowhile_end");

  break_stack_.push_back(end_lbl);
  emit_raw(body_lbl + ":");
  if (node.body) {
    node.body->accept(*this);
  }

  if (node.cond) {
    node.cond->accept(*this);
    emit_line("cbnz x0, " + body_lbl);
  }
  break_stack_.pop_back();
  emit_raw(end_lbl + ":");
}

void ARM64CodeGenerator::visit(const SwitchNode &node) {
  std::string end_lbl = new_label(".Larm_switch_end");
  std::string default_lbl = "";
  int cond_offset = get_var_offset("@arm_switch_cond_" + std::to_string(label_counter_));

  // 1. Evaluate switch condition into x0 and save in frame slot
  if (node.cond) {
    node.cond->accept(*this);
  }
  emit_line("str x0, [x29, #" + std::to_string(cond_offset) + "]");

  // 2. Generate case labels and comparison branches
  std::vector<std::string> case_labels;
  case_labels.reserve(node.cases.size());

  for (size_t i = 0; i < node.cases.size(); ++i) {
    const auto &c = node.cases[i];
    if (c->is_default) {
      std::string d_lbl = new_label(".Larm_switch_default");
      default_lbl = d_lbl;
      case_labels.push_back(d_lbl);
    } else {
      std::string c_lbl = new_label(".Larm_switch_case");
      case_labels.push_back(c_lbl);

      if (c->value) {
        c->value->accept(*this);
      }
      emit_line("ldr x1, [x29, #" + std::to_string(cond_offset) + "]");
      emit_line("cmp x1, x0");
      emit_line("b.eq " + c_lbl);
    }
  }

  // 3. Fallthrough to default or end
  if (!default_lbl.empty()) {
    emit_line("b " + default_lbl);
  } else {
    emit_line("b " + end_lbl);
  }

  // 4. Emit case bodies
  break_stack_.push_back(end_lbl);

  for (size_t i = 0; i < node.cases.size(); ++i) {
    emit_raw(case_labels[i] + ":");
    for (const auto &stmt : node.cases[i]->statements) {
      if (stmt) {
        stmt->accept(*this);
      }
    }
  }

  break_stack_.pop_back();
  emit_raw(end_lbl + ":");
}

void ARM64CodeGenerator::visit(const CaseNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void ARM64CodeGenerator::visit(const BreakNode &node) {
  if (break_stack_.empty()) {
    unsupported(node, "break statement outside of loop or switch");
  }
  emit_line("b " + break_stack_.back());
}

void ARM64CodeGenerator::visit(const ReturnNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  emit_line("b " + ret_label_);
}
