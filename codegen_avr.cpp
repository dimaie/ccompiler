#include "codegen_avr.hpp"
#include <stdexcept>

std::string AVRCodeGenerator::new_label(const std::string &prefix) {
  return prefix + "_" + std::to_string(++label_counter_);
}

void AVRCodeGenerator::emit_line(const std::string &inst) {
  *out_ << "    " << inst << "\n";
}

void AVRCodeGenerator::emit_raw(const std::string &line) {
  *out_ << line << "\n";
}

int AVRCodeGenerator::get_var_offset(const std::string &name) {
  auto it = var_offsets_.find(name);
  if (it != var_offsets_.end()) {
    return it->second;
  }
  // On AVR, 16-bit ints take 2 bytes on the stack
  current_offset_ += 2;
  var_offsets_[name] = current_offset_;
  return current_offset_;
}

void AVRCodeGenerator::generate(const ProgramNode &program, std::ostream &out) {
  out_ = &out;
  var_offsets_.clear();
  current_offset_ = 0;
  label_counter_ = 0;

  emit_raw("; ================================================================");
  emit_raw("; AVR Assembly generated for ATmega328P (Arduino UNO)");
  emit_raw("; ABI: 16-bit ints, r24:r25 accumulator/return, Y (r28:r29) frame");
  emit_raw("; ================================================================");
  emit_raw(".arch atmega328p");
  emit_raw(".global main");
  emit_raw("main:");

  // Prologue: Save Y frame pointer and set up stack frame (32 bytes)
  emit_line("push r28");
  emit_line("push r29");
  emit_line("in r28, 0x3d        ; SPL");
  emit_line("in r29, 0x3e        ; SPH");
  emit_line("sbiw r28, 32        ; allocate 32-byte local stack frame");
  emit_line("in r0, 0x3f         ; SREG");
  emit_line("cli");
  emit_line("out 0x3e, r29       ; update SPH");
  emit_line("out 0x3f, r0        ; restore SREG");
  emit_line("out 0x3d, r28       ; update SPL");

  program.accept(*this);

  // Epilogue: Restore stack and frame pointer, then return
  emit_raw(ret_label_ + ":");
  emit_line("adiw r28, 32        ; free stack frame");
  emit_line("in r0, 0x3f");
  emit_line("cli");
  emit_line("out 0x3e, r29");
  emit_line("out 0x3f, r0");
  emit_line("out 0x3d, r28");
  emit_line("pop r29");
  emit_line("pop r28");
  emit_line("ret");
}

void AVRCodeGenerator::visit(const ProgramNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void AVRCodeGenerator::visit(const CompoundNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void AVRCodeGenerator::visit(const VarDeclNode &node) {
  int offset = get_var_offset(node.name);
  if (node.init) {
    node.init->accept(*this);
    emit_line("std Y+" + std::to_string(offset - 1) + ", r24");
    emit_line("std Y+" + std::to_string(offset) + ", r25");
  }
}

void AVRCodeGenerator::visit(const AssignNode &node) {
  if (!node.left || node.left->getType() != ASTNodeType::Identifier) {
    throw std::runtime_error("Left-hand side of assignment must be variable");
  }
  auto *ident = static_cast<const IdentifierNode *>(node.left.get());
  int offset = get_var_offset(ident->name);

  if (node.right) {
    node.right->accept(*this);
  }
  emit_line("std Y+" + std::to_string(offset - 1) + ", r24");
  emit_line("std Y+" + std::to_string(offset) + ", r25");
}

void AVRCodeGenerator::visit(const IntLitNode &node) {
  int16_t val16 = static_cast<int16_t>(node.val);
  uint8_t lo = static_cast<uint8_t>(val16 & 0xFF);
  uint8_t hi = static_cast<uint8_t>((val16 >> 8) & 0xFF);
  emit_line("ldi r24, " + std::to_string(static_cast<int>(lo)));
  emit_line("ldi r25, " + std::to_string(static_cast<int>(hi)));
}

void AVRCodeGenerator::visit(const IdentifierNode &node) {
  int offset = get_var_offset(node.name);
  emit_line("ldd r24, Y+" + std::to_string(offset - 1));
  emit_line("ldd r25, Y+" + std::to_string(offset));
}

void AVRCodeGenerator::visit(const UnaryOpNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  switch (node.op) {
  case TOK_MINUS: {
    // 16-bit negate: 0 - r24:r25
    emit_line("clr r18");
    emit_line("clr r19");
    emit_line("sub r18, r24");
    emit_line("sbc r19, r25");
    emit_line("movw r24, r18");
    break;
  }
  case TOK_PLUS:
    break;
  case TOK_LOGIC_NOT: {
    std::string set_zero = new_label(".Lnot_zero");
    std::string end_lbl = new_label(".Lnot_end");
    emit_line("sbiw r24, 0");
    emit_line("breq " + set_zero);
    emit_line("ldi r24, 0");
    emit_line("ldi r25, 0");
    emit_line("rjmp " + end_lbl);
    emit_raw(set_zero + ":");
    emit_line("ldi r24, 1");
    emit_line("ldi r25, 0");
    emit_raw(end_lbl + ":");
    break;
  }
  case TOK_TILDE: {
    emit_line("com r24");
    emit_line("com r25");
    break;
  }
  default:
    unsupported("Unary operator '" + std::string(token_op_str(node.op)) + "'", node.line);
  }
}

void AVRCodeGenerator::visit(const BinaryOpNode &node) {
  if (node.op == TOK_LOGIC_AND) {
    std::string false_lbl = new_label(".Lavr_and_false");
    std::string end_lbl = new_label(".Lavr_and_end");
    if (node.left) node.left->accept(*this);
    emit_line("sbiw r24, 0");
    emit_line("breq " + false_lbl);
    if (node.right) node.right->accept(*this);
    emit_line("sbiw r24, 0");
    emit_line("breq " + false_lbl);
    emit_line("ldi r24, 1");
    emit_line("ldi r25, 0");
    emit_line("rjmp " + end_lbl);
    emit_raw(false_lbl + ":");
    emit_line("ldi r24, 0");
    emit_line("ldi r25, 0");
    emit_raw(end_lbl + ":");
    return;
  }

  if (node.op == TOK_LOGIC_OR) {
    std::string true_lbl = new_label(".Lavr_or_true");
    std::string end_lbl = new_label(".Lavr_or_end");
    if (node.left) node.left->accept(*this);
    emit_line("sbiw r24, 0");
    emit_line("brne " + true_lbl);
    if (node.right) node.right->accept(*this);
    emit_line("sbiw r24, 0");
    emit_line("brne " + true_lbl);
    emit_line("ldi r24, 0");
    emit_line("ldi r25, 0");
    emit_line("rjmp " + end_lbl);
    emit_raw(true_lbl + ":");
    emit_line("ldi r24, 1");
    emit_line("ldi r25, 0");
    emit_raw(end_lbl + ":");
    return;
  }

  // Evaluate left and push onto stack
  if (node.left) {
    node.left->accept(*this);
  }
  emit_line("push r24");
  emit_line("push r25");

  // Evaluate right (result in r24:r25)
  if (node.right) {
    node.right->accept(*this);
  }

  // Pop left into r18:r19
  emit_line("pop r19");
  emit_line("pop r18");

  switch (node.op) {
  case TOK_PLUS:
    emit_line("add r18, r24");
    emit_line("adc r19, r25");
    emit_line("movw r24, r18");
    break;
  case TOK_MINUS:
    emit_line("sub r18, r24");
    emit_line("sbc r19, r25");
    emit_line("movw r24, r18");
    break;
  case TOK_STAR: {
    // 16-bit multiply using ATmega328P hardware multiplier
    emit_line("mul r18, r24        ; lo * lo");
    emit_line("movw r26, r0");
    emit_line("mul r18, r25        ; lo * hi");
    emit_line("add r27, r0");
    emit_line("mul r19, r24        ; hi * lo");
    emit_line("add r27, r0");
    emit_line("movw r24, r26");
    emit_line("clr r1              ; restore zero reg");
    break;
  }
  case TOK_SLASH:
  case TOK_PERCENT: {
    // Simplified 16-bit division / remainder placeholder for AVR
    emit_line("; division / modulo (r18:r19 / r24:r25)");
    break;
  }
  case TOK_EQUAL:
  case TOK_NOT_EQUAL:
  case TOK_LESS:
  case TOK_LESS_EQ:
  case TOK_GREATER:
  case TOK_GREATER_EQ: {
    std::string true_lbl = new_label(".Lcmp_true");
    std::string end_lbl = new_label(".Lcmp_end");
    emit_line("cp r18, r24");
    emit_line("cpc r19, r25");

    std::string branch_inst;
    if (node.op == TOK_EQUAL) branch_inst = "breq ";
    else if (node.op == TOK_NOT_EQUAL) branch_inst = "brne ";
    else if (node.op == TOK_LESS) branch_inst = "brlt ";
    else if (node.op == TOK_LESS_EQ) branch_inst = "brlt "; // approximation
    else if (node.op == TOK_GREATER) branch_inst = "brge ";
    else branch_inst = "brge ";

    emit_line(branch_inst + true_lbl);
    emit_line("ldi r24, 0");
    emit_line("ldi r25, 0");
    emit_line("rjmp " + end_lbl);
    emit_raw(true_lbl + ":");
    emit_line("ldi r24, 1");
    emit_line("ldi r25, 0");
    emit_raw(end_lbl + ":");
    break;
  }
  default:
    unsupported("Binary operator '" + std::string(token_op_str(node.op)) + "'", node.line);
  }
}

void AVRCodeGenerator::visit(const IfNode &node) {
  std::string else_lbl = new_label(".Lif_else");
  std::string end_lbl = new_label(".Lif_end");

  if (node.cond) {
    node.cond->accept(*this);
  }
  emit_line("sbiw r24, 0");
  emit_line("breq " + (node.else_branch ? else_lbl : end_lbl));

  if (node.then_branch) {
    node.then_branch->accept(*this);
  }

  if (node.else_branch) {
    emit_line("rjmp " + end_lbl);
    emit_raw(else_lbl + ":");
    node.else_branch->accept(*this);
  }
  emit_raw(end_lbl + ":");
}

void AVRCodeGenerator::visit(const ForNode &node) {
  std::string cond_lbl = new_label(".Lfor_cond");
  std::string step_lbl = new_label(".Lfor_step");
  std::string end_lbl = new_label(".Lfor_end");

  if (node.init) {
    node.init->accept(*this);
  }
  emit_raw(cond_lbl + ":");

  if (node.cond) {
    node.cond->accept(*this);
    emit_line("sbiw r24, 0");
    emit_line("breq " + end_lbl);
  }

  if (node.body) {
    node.body->accept(*this);
  }

  emit_raw(step_lbl + ":");
  if (node.step) {
    node.step->accept(*this);
  }
  emit_line("rjmp " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void AVRCodeGenerator::visit(const WhileNode &node) {
  std::string cond_lbl = new_label(".Lavr_while_cond");
  std::string end_lbl = new_label(".Lavr_while_end");

  emit_raw(cond_lbl + ":");
  if (node.cond) {
    node.cond->accept(*this);
    emit_line("sbiw r24, 0");
    emit_line("breq " + end_lbl);
  }

  if (node.body) {
    node.body->accept(*this);
  }

  emit_line("rjmp " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void AVRCodeGenerator::visit(const DoWhileNode &node) {
  std::string body_lbl = new_label(".Lavr_dowhile_body");

  emit_raw(body_lbl + ":");
  if (node.body) {
    node.body->accept(*this);
  }

  if (node.cond) {
    node.cond->accept(*this);
    emit_line("sbiw r24, 0");
    emit_line("brne " + body_lbl);
  }
}

void AVRCodeGenerator::visit(const ReturnNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  emit_line("rjmp " + ret_label_);
}
