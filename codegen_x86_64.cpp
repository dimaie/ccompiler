#include "codegen_x86_64.hpp"
#include <stdexcept>

std::string X86_64CodeGenerator::new_label(const std::string &prefix) {
  return prefix + "_" + std::to_string(++label_counter_);
}

void X86_64CodeGenerator::emit_line(const std::string &inst) {
  *out_ << "    " << inst << "\n";
}

void X86_64CodeGenerator::emit_raw(const std::string &line) {
  *out_ << line << "\n";
}

int X86_64CodeGenerator::get_var_offset(const std::string &name) {
  auto it = var_offsets_.find(name);
  if (it != var_offsets_.end()) {
    return it->second;
  }
  current_offset_ += 8;
  var_offsets_[name] = current_offset_;
  return current_offset_;
}

DataType X86_64CodeGenerator::get_var_type(const std::string &name) const {
  auto it = var_types_.find(name);
  if (it != var_types_.end()) {
    return it->second;
  }
  return DataType::Int;
}

static int compute_ast_frame_bytes(const ASTNode *node) {
  if (!node) return 0;
  int size = 0;
  if (node->getType() == ASTNodeType::VarDecl) {
    const auto *v = static_cast<const VarDeclNode *>(node);
    if (v->is_array) {
      size += v->array_size * 8;
    } else {
      size += 8;
    }
  } else if (node->getType() == ASTNodeType::Compound) {
    const auto *c = static_cast<const CompoundNode *>(node);
    for (const auto &s : c->statements) {
      size += compute_ast_frame_bytes(s.get());
    }
  } else if (node->getType() == ASTNodeType::If) {
    const auto *i = static_cast<const IfNode *>(node);
    size += compute_ast_frame_bytes(i->then_branch.get());
    size += compute_ast_frame_bytes(i->else_branch.get());
  } else if (node->getType() == ASTNodeType::For) {
    const auto *f = static_cast<const ForNode *>(node);
    size += compute_ast_frame_bytes(f->init.get());
    size += compute_ast_frame_bytes(f->body.get());
  } else if (node->getType() == ASTNodeType::While) {
    const auto *w = static_cast<const WhileNode *>(node);
    size += compute_ast_frame_bytes(w->body.get());
  } else if (node->getType() == ASTNodeType::DoWhile) {
    const auto *d = static_cast<const DoWhileNode *>(node);
    size += compute_ast_frame_bytes(d->body.get());
  } else if (node->getType() == ASTNodeType::Switch) {
    const auto *s = static_cast<const SwitchNode *>(node);
    size += 8;
    for (const auto &c : s->cases) {
      for (const auto &st : c->statements) {
        size += compute_ast_frame_bytes(st.get());
      }
    }
  }
  return size;
}

void X86_64CodeGenerator::generate(const ProgramNode &program,
                                   std::ostream &out) {
  out_ = &out;
  var_offsets_.clear();
  var_types_.clear();
  is_array_.clear();
  array_sizes_.clear();
  current_offset_ = 0;
  label_counter_ = 0;
  break_stack_.clear();
  continue_stack_.clear();

  bool has_func_defs = false;
  bool has_main_func = false;
  std::vector<const ASTNode *> non_func_stmts;
  for (const auto &stmt : program.statements) {
    if (stmt) {
      if (stmt->getType() == ASTNodeType::FuncDef) {
        has_func_defs = true;
        const auto *f = static_cast<const FuncDefNode *>(stmt.get());
        if (f->name == "main") {
          has_main_func = true;
        }
      } else {
        non_func_stmts.push_back(stmt.get());
      }
    }
  }

  if (!has_func_defs) {
    int extra = 0;
    for (const auto &stmt : program.statements) {
      extra += compute_ast_frame_bytes(stmt.get());
    }
    int frame_size = (extra + 16 > 128) ? ((extra + 16 + 15) & ~15) : 128;

    ret_label_ = ".Lmain_ret";
    emit_raw(".globl main");
    emit_raw("main:");
    emit_line("pushq %rbp");
    emit_line("movq %rsp, %rbp");
    emit_line("subq $" + std::to_string(frame_size) + ", %rsp");

    program.accept(*this);

    emit_raw(ret_label_ + ":");
    emit_line("leave");
    emit_line("ret");
    return;
  }

  for (const auto &stmt : program.statements) {
    if (stmt && stmt->getType() == ASTNodeType::FuncDef) {
      stmt->accept(*this);
    }
  }

  if (!has_main_func && !non_func_stmts.empty()) {
    var_offsets_.clear();
    var_types_.clear();
    is_array_.clear();
    array_sizes_.clear();
    current_offset_ = 0;

    int extra = 0;
    for (const auto *stmt : non_func_stmts) {
      extra += compute_ast_frame_bytes(stmt);
    }
    int frame_size = (extra + 16 > 128) ? ((extra + 16 + 15) & ~15) : 128;

    ret_label_ = ".Lmain_ret";
    emit_raw(".globl main");
    emit_raw("main:");
    emit_line("pushq %rbp");
    emit_line("movq %rsp, %rbp");
    emit_line("subq $" + std::to_string(frame_size) + ", %rsp");

    for (const auto *stmt : non_func_stmts) {
      stmt->accept(*this);
    }

    emit_raw(ret_label_ + ":");
    emit_line("leave");
    emit_line("ret");
  }
}

void X86_64CodeGenerator::visit(const FuncDefNode &node) {
  var_offsets_.clear();
  var_types_.clear();
  is_array_.clear();
  array_sizes_.clear();
  current_offset_ = 0;
  break_stack_.clear();
  continue_stack_.clear();

  std::string prev_ret_label = ret_label_;
  ret_label_ = ".L" + node.name + "_ret";

  int extra = static_cast<int>(node.params.size()) * 8 +
              compute_ast_frame_bytes(node.body.get());
  int frame_size = (extra + 16 > 128) ? ((extra + 16 + 15) & ~15) : 128;

  emit_raw(".globl " + node.name);
  emit_raw(node.name + ":");
  emit_line("pushq %rbp");
  emit_line("movq %rsp, %rbp");
  emit_line("subq $" + std::to_string(frame_size) + ", %rsp");

  static const char *const ARG_REGS[] = {
      "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

  for (size_t i = 0; i < node.params.size(); ++i) {
    if (i < 6) {
      const auto &p = node.params[i];
      var_types_[p.name] = p.type;
      int offset = get_var_offset(p.name);
      emit_line("movq " + std::string(ARG_REGS[i]) + ", -" +
                std::to_string(offset) + "(%rbp)");
    } else {
      unsupported(node, "more than 6 function parameters");
    }
  }

  if (node.body) {
    node.body->accept(*this);
  }

  emit_raw(ret_label_ + ":");
  emit_line("leave");
  emit_line("ret");

  ret_label_ = prev_ret_label;
}

void X86_64CodeGenerator::visit(const CallNode &node) {
  if (node.args.size() > 6) {
    unsupported(node, "more than 6 function call arguments");
  }

  for (const auto &arg : node.args) {
    if (arg) {
      arg->accept(*this);
    } else {
      emit_line("movq $0, %rax");
    }
    emit_line("pushq %rax");
  }

  static const char *const ARG_REGS[] = {
      "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
  for (int i = static_cast<int>(node.args.size()) - 1; i >= 0; --i) {
    emit_line("popq " + std::string(ARG_REGS[i]));
  }

  emit_line("movq %rsp, %r11");
  emit_line("andq $-16, %rsp");
  emit_line("pushq %r11");
  emit_line("subq $8, %rsp");
  emit_line("call " + node.callee);
  emit_line("addq $8, %rsp");
  emit_line("popq %rsp");
}

void X86_64CodeGenerator::visit(const ProgramNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void X86_64CodeGenerator::visit(const CompoundNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void X86_64CodeGenerator::visit(const VarDeclNode &node) {
  var_types_[node.name] = node.data_type;
  if (node.is_array) {
    is_array_[node.name] = true;
    array_sizes_[node.name] = node.array_size;
    int elem_size = (node.data_type == DataType::Short) ? 2 : 8;
    current_offset_ += node.array_size * elem_size;
    if (current_offset_ % 8 != 0) {
      current_offset_ += 8 - (current_offset_ % 8);
    }
    int base_offset = current_offset_;
    var_offsets_[node.name] = base_offset;

    if (node.init) {
      const auto *init_list = static_cast<const InitListNode *>(node.init.get());
      for (size_t i = 0; i < init_list->elements.size(); ++i) {
        if (init_list->elements[i]) {
          init_list->elements[i]->accept(*this);
          int elem_offset = base_offset - static_cast<int>(i) * elem_size;
          if (node.data_type == DataType::Short) {
            emit_line("movw %ax, -" + std::to_string(elem_offset) + "(%rbp)");
          } else {
            emit_line("movq %rax, -" + std::to_string(elem_offset) + "(%rbp)");
          }
        }
      }
    }
  } else {
    int offset = get_var_offset(node.name);
    if (node.init) {
      node.init->accept(*this);
      if (node.data_type == DataType::Short) {
        emit_line("movw %ax, -" + std::to_string(offset) + "(%rbp)");
      } else {
        emit_line("movq %rax, -" + std::to_string(offset) + "(%rbp)");
      }
    }
  }
}

void X86_64CodeGenerator::visit(const ArrayAccessNode &node) {
  if (!node.array || node.array->getType() != ASTNodeType::Identifier) {
    unsupported(node, "complex array access");
  }
  auto *ident = static_cast<const IdentifierNode *>(node.array.get());
  int base_offset = get_var_offset(ident->name);

  if (node.index) {
    node.index->accept(*this);
  } else {
    emit_line("movq $0, %rax");
  }

  emit_line("leaq -" + std::to_string(base_offset) + "(%rbp), %r11");
  if (get_var_type(ident->name) == DataType::Short) {
    emit_line("movswq (%r11, %rax, 2), %rax");
  } else {
    emit_line("movq (%r11, %rax, 8), %rax");
  }
}

void X86_64CodeGenerator::visit(const InitListNode &node) {
  for (const auto &elem : node.elements) {
    if (elem) {
      elem->accept(*this);
    }
  }
}

void X86_64CodeGenerator::visit(const AssignNode &node) {
  if (!node.left) {
    throw std::runtime_error("Left-hand side of assignment cannot be null");
  }

  if (node.left->getType() == ASTNodeType::ArrayAccess) {
    auto *acc = static_cast<const ArrayAccessNode *>(node.left.get());
    if (!acc->array || acc->array->getType() != ASTNodeType::Identifier) {
      unsupported(node, "complex array assignment");
    }
    auto *ident = static_cast<const IdentifierNode *>(acc->array.get());
    int base_offset = get_var_offset(ident->name);

    if (acc->index) {
      acc->index->accept(*this);
    } else {
      emit_line("movq $0, %rax");
    }
    emit_line("pushq %rax");

    if (node.right) {
      node.right->accept(*this);
    }

    emit_line("popq %r10");
    emit_line("leaq -" + std::to_string(base_offset) + "(%rbp), %r11");
    if (get_var_type(ident->name) == DataType::Short) {
      emit_line("movw %ax, (%r11, %r10, 2)");
    } else {
      emit_line("movq %rax, (%r11, %r10, 8)");
    }
    return;
  }

  if (node.left->getType() != ASTNodeType::Identifier) {
    throw std::runtime_error("Left-hand side of assignment must be variable or array element");
  }
  auto *ident = static_cast<const IdentifierNode *>(node.left.get());
  int offset = get_var_offset(ident->name);

  if (node.right) {
    node.right->accept(*this);
  }
  if (get_var_type(ident->name) == DataType::Short) {
    emit_line("movw %ax, -" + std::to_string(offset) + "(%rbp)");
  } else {
    emit_line("movq %rax, -" + std::to_string(offset) + "(%rbp)");
  }
}

void X86_64CodeGenerator::visit(const IntLitNode &node) {
  emit_line("movq $" + std::to_string(node.val) + ", %rax");
}

void X86_64CodeGenerator::visit(const IdentifierNode &node) {
  int offset = get_var_offset(node.name);
  auto it = is_array_.find(node.name);
  if (it != is_array_.end() && it->second) {
    emit_line("leaq -" + std::to_string(offset) + "(%rbp), %rax");
    return;
  }
  if (get_var_type(node.name) == DataType::Short) {
    emit_line("movswq -" + std::to_string(offset) + "(%rbp), %rax");
  } else {
    emit_line("movq -" + std::to_string(offset) + "(%rbp), %rax");
  }
}

void X86_64CodeGenerator::visit(const UnaryOpNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  switch (node.op) {
  case TOK_MINUS:
    emit_line("negq %rax");
    break;
  case TOK_PLUS:
    break;
  case TOK_LOGIC_NOT:
    emit_line("cmpq $0, %rax");
    emit_line("sete %al");
    emit_line("movzbq %al, %rax");
    break;
  case TOK_TILDE:
    emit_line("notq %rax");
    break;
  default:
    unsupported("Unary operator '" + std::string(token_op_str(node.op)) + "'", node.line);
  }
}

void X86_64CodeGenerator::visit(const BinaryOpNode &node) {
  if (node.op == TOK_LOGIC_AND) {
    std::string false_lbl = new_label(".Land_false");
    std::string end_lbl = new_label(".Land_end");
    if (node.left) node.left->accept(*this);
    emit_line("cmpq $0, %rax");
    emit_line("je " + false_lbl);
    if (node.right) node.right->accept(*this);
    emit_line("cmpq $0, %rax");
    emit_line("je " + false_lbl);
    emit_line("movq $1, %rax");
    emit_line("jmp " + end_lbl);
    emit_raw(false_lbl + ":");
    emit_line("movq $0, %rax");
    emit_raw(end_lbl + ":");
    return;
  }

  if (node.op == TOK_LOGIC_OR) {
    std::string true_lbl = new_label(".Lor_true");
    std::string end_lbl = new_label(".Lor_end");
    if (node.left) node.left->accept(*this);
    emit_line("cmpq $0, %rax");
    emit_line("jne " + true_lbl);
    if (node.right) node.right->accept(*this);
    emit_line("cmpq $0, %rax");
    emit_line("jne " + true_lbl);
    emit_line("movq $0, %rax");
    emit_line("jmp " + end_lbl);
    emit_raw(true_lbl + ":");
    emit_line("movq $1, %rax");
    emit_raw(end_lbl + ":");
    return;
  }

  if (node.left) {
    node.left->accept(*this);
  }
  emit_line("pushq %rax");

  if (node.right) {
    node.right->accept(*this);
  }
  emit_line("movq %rax, %r10");
  emit_line("popq %rax");

  switch (node.op) {
  case TOK_PLUS:
    emit_line("addq %r10, %rax");
    break;
  case TOK_MINUS:
    emit_line("subq %r10, %rax");
    break;
  case TOK_STAR:
    emit_line("imulq %r10, %rax");
    break;
  case TOK_SLASH:
    emit_line("cqto");
    emit_line("idivq %r10");
    break;
  case TOK_PERCENT:
    emit_line("cqto");
    emit_line("idivq %r10");
    emit_line("movq %rdx, %rax");
    break;
  case TOK_EQUAL:
    emit_line("cmpq %r10, %rax");
    emit_line("sete %al");
    emit_line("movzbq %al, %rax");
    break;
  case TOK_NOT_EQUAL:
    emit_line("cmpq %r10, %rax");
    emit_line("setne %al");
    emit_line("movzbq %al, %rax");
    break;
  case TOK_LESS:
    emit_line("cmpq %r10, %rax");
    emit_line("setl %al");
    emit_line("movzbq %al, %rax");
    break;
  case TOK_LESS_EQ:
    emit_line("cmpq %r10, %rax");
    emit_line("setle %al");
    emit_line("movzbq %al, %rax");
    break;
  case TOK_GREATER:
    emit_line("cmpq %r10, %rax");
    emit_line("setg %al");
    emit_line("movzbq %al, %rax");
    break;
  case TOK_GREATER_EQ:
    emit_line("cmpq %r10, %rax");
    emit_line("setge %al");
    emit_line("movzbq %al, %rax");
    break;
  default:
    unsupported("Binary operator '" + std::string(token_op_str(node.op)) + "'", node.line);
  }
}

void X86_64CodeGenerator::visit(const IfNode &node) {
  std::string else_lbl = new_label(".Lelse");
  std::string end_lbl = new_label(".Lif_end");

  if (node.cond) {
    node.cond->accept(*this);
  }
  emit_line("cmpq $0, %rax");
  emit_line("je " + (node.else_branch ? else_lbl : end_lbl));

  if (node.then_branch) {
    node.then_branch->accept(*this);
  }

  if (node.else_branch) {
    emit_line("jmp " + end_lbl);
    emit_raw(else_lbl + ":");
    node.else_branch->accept(*this);
  }
  emit_raw(end_lbl + ":");
}

void X86_64CodeGenerator::visit(const ForNode &node) {
  std::string cond_lbl = new_label(".Lfor_cond");
  std::string step_lbl = new_label(".Lfor_step");
  std::string end_lbl = new_label(".Lfor_end");

  if (node.init) {
    node.init->accept(*this);
  }
  emit_raw(cond_lbl + ":");

  if (node.cond) {
    node.cond->accept(*this);
    emit_line("cmpq $0, %rax");
    emit_line("je " + end_lbl);
  }

  break_stack_.push_back(end_lbl);
  continue_stack_.push_back(step_lbl);
  if (node.body) {
    node.body->accept(*this);
  }
  continue_stack_.pop_back();
  break_stack_.pop_back();

  emit_raw(step_lbl + ":");
  if (node.step) {
    node.step->accept(*this);
  }
  emit_line("jmp " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void X86_64CodeGenerator::visit(const WhileNode &node) {
  std::string cond_lbl = new_label(".Lwhile_cond");
  std::string end_lbl = new_label(".Lwhile_end");

  emit_raw(cond_lbl + ":");
  if (node.cond) {
    node.cond->accept(*this);
    emit_line("cmpq $0, %rax");
    emit_line("je " + end_lbl);
  }

  break_stack_.push_back(end_lbl);
  continue_stack_.push_back(cond_lbl);
  if (node.body) {
    node.body->accept(*this);
  }
  continue_stack_.pop_back();
  break_stack_.pop_back();

  emit_line("jmp " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void X86_64CodeGenerator::visit(const DoWhileNode &node) {
  std::string body_lbl = new_label(".Ldowhile_body");
  std::string cond_lbl = new_label(".Ldowhile_cond");
  std::string end_lbl = new_label(".Ldowhile_end");

  break_stack_.push_back(end_lbl);
  continue_stack_.push_back(cond_lbl);
  emit_raw(body_lbl + ":");
  if (node.body) {
    node.body->accept(*this);
  }
  continue_stack_.pop_back();

  emit_raw(cond_lbl + ":");
  if (node.cond) {
    node.cond->accept(*this);
    emit_line("cmpq $0, %rax");
    emit_line("jne " + body_lbl);
  }
  break_stack_.pop_back();
  emit_raw(end_lbl + ":");
}

void X86_64CodeGenerator::visit(const SwitchNode &node) {
  std::string end_lbl = new_label(".Lswitch_end");
  std::string default_lbl = "";
  int cond_offset = get_var_offset("@switch_cond_" + std::to_string(label_counter_));

  // 1. Evaluate switch condition into %rax and save in frame slot
  if (node.cond) {
    node.cond->accept(*this);
  }
  emit_line("movq %rax, -" + std::to_string(cond_offset) + "(%rbp)");

  // 2. Generate case labels and comparison branches
  std::vector<std::string> case_labels;
  case_labels.reserve(node.cases.size());

  for (size_t i = 0; i < node.cases.size(); ++i) {
    const auto &c = node.cases[i];
    if (c->is_default) {
      std::string d_lbl = new_label(".Lswitch_default");
      default_lbl = d_lbl;
      case_labels.push_back(d_lbl);
    } else {
      std::string c_lbl = new_label(".Lswitch_case");
      case_labels.push_back(c_lbl);

      if (c->value) {
        c->value->accept(*this);
      }
      emit_line("movq %rax, %r10");
      emit_line("movq -" + std::to_string(cond_offset) + "(%rbp), %rax");
      emit_line("cmpq %r10, %rax");
      emit_line("je " + c_lbl);
    }
  }

  // 3. Fallthrough to default or end
  if (!default_lbl.empty()) {
    emit_line("jmp " + default_lbl);
  } else {
    emit_line("jmp " + end_lbl);
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

void X86_64CodeGenerator::visit(const CaseNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void X86_64CodeGenerator::visit(const BreakNode &node) {
  if (break_stack_.empty()) {
    unsupported(node, "break statement outside of loop or switch");
  }
  emit_line("jmp " + break_stack_.back());
}

void X86_64CodeGenerator::visit(const ContinueNode &node) {
  if (continue_stack_.empty()) {
    unsupported(node, "continue statement outside of loop");
  }
  emit_line("jmp " + continue_stack_.back());
}

void X86_64CodeGenerator::visit(const GotoNode &node) {
  emit_line("jmp .Luser_" + node.label);
}

void X86_64CodeGenerator::visit(const LabelNode &node) {
  emit_raw(".Luser_" + node.name + ":");
}

void X86_64CodeGenerator::visit(const ReturnNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  emit_line("jmp " + ret_label_);
}
