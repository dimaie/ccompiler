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

void X86_64CodeGenerator::generate(const ProgramNode &program,
                                   std::ostream &out) {
  out_ = &out;
  var_offsets_.clear();
  current_offset_ = 0;
  label_counter_ = 0;

  emit_raw(".globl main");
  emit_raw("main:");
  emit_line("pushq %rbp");
  emit_line("movq %rsp, %rbp");
  emit_line("subq $128, %rsp");

  program.accept(*this);

  emit_raw(ret_label_ + ":");
  emit_line("leave");
  emit_line("ret");
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
  int offset = get_var_offset(node.name);
  if (node.init) {
    node.init->accept(*this);
    emit_line("movq %rax, -" + std::to_string(offset) + "(%rbp)");
  }
}

void X86_64CodeGenerator::visit(const AssignNode &node) {
  if (!node.left || node.left->getType() != ASTNodeType::Identifier) {
    throw std::runtime_error("Left-hand side of assignment must be variable");
  }
  auto *ident = static_cast<const IdentifierNode *>(node.left.get());
  int offset = get_var_offset(ident->name);

  if (node.right) {
    node.right->accept(*this);
  }
  emit_line("movq %rax, -" + std::to_string(offset) + "(%rbp)");
}

void X86_64CodeGenerator::visit(const IntLitNode &node) {
  emit_line("movq $" + std::to_string(node.val) + ", %rax");
}

void X86_64CodeGenerator::visit(const IdentifierNode &node) {
  int offset = get_var_offset(node.name);
  emit_line("movq -" + std::to_string(offset) + "(%rbp), %rax");
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
    break;
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
    break;
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

  if (node.body) {
    node.body->accept(*this);
  }

  emit_raw(step_lbl + ":");
  if (node.step) {
    node.step->accept(*this);
  }
  emit_line("jmp " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void X86_64CodeGenerator::visit(const ReturnNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  emit_line("jmp " + ret_label_);
}
