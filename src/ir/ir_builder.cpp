#include "ir_builder.hpp"
#include <stdexcept>

IROperand IRBuilder::new_var() {
  return IROperand::var(next_var_id_++);
}

std::string IRBuilder::new_label(const std::string &prefix) {
  return "." + prefix + "_" + std::to_string(++next_label_id_);
}

void IRBuilder::emit_instruction(IRInstruction inst) {
  if (current_func_) {
    current_func_->instructions.push_back(std::move(inst));
  }
}

IROperand IRBuilder::lower_expression(const ASTNode &node) {
  last_expr_operand_ = IROperand::none();
  const_cast<ASTNode &>(node).accept(*this);
  return last_expr_operand_;
}

IRModule IRBuilder::build(const ProgramNode &program) {
  module_ = IRModule{};
  current_func_ = nullptr;
  next_var_id_ = 0;
  next_label_id_ = 0;
  var_table_.clear();
  break_labels_.clear();
  continue_labels_.clear();

  program.accept(*this);
  return std::move(module_);
}

void IRBuilder::visit(const ProgramNode &node) {
  bool has_func_defs = false;
  bool has_main_func = false;
  std::vector<const ASTNode *> non_func_stmts;

  for (const auto &stmt : node.statements) {
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

  // Visit function definitions
  for (const auto &stmt : node.statements) {
    if (stmt && stmt->getType() == ASTNodeType::FuncDef) {
      stmt->accept(*this);
    }
  }

  // Synthesize main if top-level non-function statements exist
  if (!has_func_defs || (!has_main_func && !non_func_stmts.empty())) {
    IRFunction synth_main;
    synth_main.name = "main";
    synth_main.return_type = DataType::Int;
    synth_main.is_static = false;

    current_func_ = &synth_main;
    var_table_.clear();

    for (const auto &stmt : node.statements) {
      if (stmt && stmt->getType() != ASTNodeType::FuncDef) {
        const_cast<ASTNode *>(stmt.get())->accept(*this);
      }
    }

    // Default return 0 if no explicit return
    if (synth_main.instructions.empty() || synth_main.instructions.back().op != IROpKind::Return) {
      IRInstruction ret_inst;
      ret_inst.op = IROpKind::Return;
      ret_inst.src1 = IROperand::imm(0);
      synth_main.instructions.push_back(ret_inst);
    }

    current_func_ = nullptr;
    module_.functions.push_back(std::move(synth_main));
  }
}

void IRBuilder::visit(const FuncDefNode &node) {
  if (!node.body) {
    return; // Function prototype
  }

  IRFunction fn;
  fn.name = node.name;
  fn.is_static = node.is_static;
  fn.return_type = node.return_type;

  var_table_.clear();
  current_func_ = &fn;

  // Parameters
  for (const auto &param : node.params) {
    IROperand param_var = new_var();
    fn.params.push_back(param_var);
    var_table_[param.name] = param_var;
  }

  node.body->accept(*this);

  // Ensure function ends with a return
  if (fn.instructions.empty() || fn.instructions.back().op != IROpKind::Return) {
    IRInstruction ret_inst;
    ret_inst.op = IROpKind::Return;
    if (node.return_type == DataType::Void) {
      ret_inst.src1 = IROperand::none();
    } else {
      ret_inst.src1 = IROperand::imm(0);
    }
    fn.instructions.push_back(ret_inst);
  }

  current_func_ = nullptr;
  module_.functions.push_back(std::move(fn));
}

void IRBuilder::visit(const CompoundNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void IRBuilder::visit(const VarDeclNode &node) {
  IROperand var_op = new_var();
  var_table_[node.name] = var_op;

  if (node.init) {
    IROperand init_op = lower_expression(*node.init);
    IRInstruction copy_inst;
    copy_inst.op = IROpKind::Copy;
    copy_inst.dst = var_op;
    copy_inst.src1 = init_op;
    copy_inst.comment = "var " + node.name;
    emit_instruction(copy_inst);
  }
}

void IRBuilder::visit(const ReturnNode &node) {
  IRInstruction ret_inst;
  ret_inst.op = IROpKind::Return;
  if (node.expr) {
    ret_inst.src1 = lower_expression(*node.expr);
  }
  emit_instruction(ret_inst);
}

void IRBuilder::visit(const IntLitNode &node) {
  last_expr_operand_ = IROperand::imm(node.val);
}

void IRBuilder::visit(const StringLitNode &node) {
  std::string str_label = new_label("str");
  module_.string_literals.push_back({str_label, node.value});
  last_expr_operand_ = IROperand::global(str_label);
}

void IRBuilder::visit(const IdentifierNode &node) {
  auto it = var_table_.find(node.name);
  if (it != var_table_.end()) {
    last_expr_operand_ = it->second;
  } else {
    // Global or function reference
    last_expr_operand_ = IROperand::global(node.name);
  }
}

void IRBuilder::visit(const AssignNode &node) {
  IROperand rhs = lower_expression(*node.right);

  // If assigning to a simple identifier
  if (node.left && node.left->getType() == ASTNodeType::Identifier) {
    const auto *id = static_cast<const IdentifierNode *>(node.left.get());
    auto it = var_table_.find(id->name);
    IROperand dst_op;
    if (it != var_table_.end()) {
      dst_op = it->second;
    } else {
      dst_op = new_var();
      var_table_[id->name] = dst_op;
    }

    IRInstruction copy_inst;
    copy_inst.op = IROpKind::Copy;
    copy_inst.dst = dst_op;
    copy_inst.src1 = rhs;
    emit_instruction(copy_inst);
    last_expr_operand_ = dst_op;
  } else {
    last_expr_operand_ = rhs;
  }
}

void IRBuilder::visit(const BinaryOpNode &node) {
  // Short-circuit logical AND (&&)
  if (node.op == TOK_LOGIC_AND) {
    IROperand res_var = new_var();
    std::string false_lbl = new_label("and_false");
    std::string end_lbl = new_label("and_end");

    IROperand left_op = lower_expression(*node.left);
    IRInstruction br_left;
    br_left.op = IROpKind::BranchIf;
    br_left.src1 = left_op;
    br_left.dst = IROperand::label(end_lbl); // if true, fallthrough/continue
    br_left.src2 = IROperand::label(false_lbl);
    emit_instruction(br_left);

    IROperand right_op = lower_expression(*node.right);
    IRInstruction copy_r;
    copy_r.op = IROpKind::Copy;
    copy_r.dst = res_var;
    copy_r.src1 = right_op;
    emit_instruction(copy_r);

    IRInstruction jmp_end;
    jmp_end.op = IROpKind::Jump;
    jmp_end.dst = IROperand::label(end_lbl);
    emit_instruction(jmp_end);

    // False label
    IRInstruction lbl_f;
    lbl_f.op = IROpKind::Label;
    lbl_f.dst = IROperand::label(false_lbl);
    emit_instruction(lbl_f);

    IRInstruction copy_0;
    copy_0.op = IROpKind::Copy;
    copy_0.dst = res_var;
    copy_0.src1 = IROperand::imm(0);
    emit_instruction(copy_0);

    // End label
    IRInstruction lbl_end;
    lbl_end.op = IROpKind::Label;
    lbl_end.dst = IROperand::label(end_lbl);
    emit_instruction(lbl_end);

    last_expr_operand_ = res_var;
    return;
  }

  // Short-circuit logical OR (||)
  if (node.op == TOK_LOGIC_OR) {
    IROperand res_var = new_var();
    std::string true_lbl = new_label("or_true");
    std::string end_lbl = new_label("or_end");

    IROperand left_op = lower_expression(*node.left);
    IRInstruction br_left;
    br_left.op = IROpKind::BranchIf;
    br_left.src1 = left_op;
    br_left.dst = IROperand::label(true_lbl);
    br_left.src2 = IROperand::label(end_lbl);
    emit_instruction(br_left);

    IROperand right_op = lower_expression(*node.right);
    IRInstruction copy_r;
    copy_r.op = IROpKind::Copy;
    copy_r.dst = res_var;
    copy_r.src1 = right_op;
    emit_instruction(copy_r);

    IRInstruction jmp_end;
    jmp_end.op = IROpKind::Jump;
    jmp_end.dst = IROperand::label(end_lbl);
    emit_instruction(jmp_end);

    // True label
    IRInstruction lbl_t;
    lbl_t.op = IROpKind::Label;
    lbl_t.dst = IROperand::label(true_lbl);
    emit_instruction(lbl_t);

    IRInstruction copy_1;
    copy_1.op = IROpKind::Copy;
    copy_1.dst = res_var;
    copy_1.src1 = IROperand::imm(1);
    emit_instruction(copy_1);

    // End label
    IRInstruction lbl_end;
    lbl_end.op = IROpKind::Label;
    lbl_end.dst = IROperand::label(end_lbl);
    emit_instruction(lbl_end);

    last_expr_operand_ = res_var;
    return;
  }

  IROperand left_op = lower_expression(*node.left);
  IROperand right_op = lower_expression(*node.right);
  IROperand dst_op = new_var();

  IRInstruction inst;
  inst.dst = dst_op;
  inst.src1 = left_op;
  inst.src2 = right_op;

  if (node.op == TOK_PLUS) inst.op = IROpKind::Add;
  else if (node.op == TOK_MINUS) inst.op = IROpKind::Sub;
  else if (node.op == TOK_STAR) inst.op = IROpKind::Mul;
  else if (node.op == TOK_SLASH) inst.op = IROpKind::Div;
  else if (node.op == TOK_PERCENT) inst.op = IROpKind::Mod;
  else if (node.op == TOK_AMP) inst.op = IROpKind::And;
  else if (node.op == TOK_PIPE) inst.op = IROpKind::Or;
  else if (node.op == TOK_CARET) inst.op = IROpKind::Xor;
  else if (node.op == TOK_LSHIFT) inst.op = IROpKind::Shl;
  else if (node.op == TOK_RSHIFT) inst.op = IROpKind::Shr;
  else if (node.op == TOK_EQUAL) inst.op = IROpKind::Eq;
  else if (node.op == TOK_NOT_EQUAL) inst.op = IROpKind::Ne;
  else if (node.op == TOK_LESS) inst.op = IROpKind::Lt;
  else if (node.op == TOK_LESS_EQ) inst.op = IROpKind::Le;
  else if (node.op == TOK_GREATER) inst.op = IROpKind::Gt;
  else if (node.op == TOK_GREATER_EQ) inst.op = IROpKind::Ge;
  else inst.op = IROpKind::Add;

  emit_instruction(inst);
  last_expr_operand_ = dst_op;
}

void IRBuilder::visit(const UnaryOpNode &node) {
  IROperand operand = lower_expression(*node.expr);
  IROperand dst_op = new_var();

  IRInstruction inst;
  inst.dst = dst_op;
  inst.src1 = operand;

  if (node.op == TOK_MINUS) {
    inst.op = IROpKind::Neg;
  } else if (node.op == TOK_LOGIC_NOT) {
    inst.op = IROpKind::Not;
  } else if (node.op == TOK_TILDE) {
    inst.op = IROpKind::Not;
  } else {
    inst.op = IROpKind::Copy;
  }

  emit_instruction(inst);
  last_expr_operand_ = dst_op;
}

void IRBuilder::visit(const CallNode &node) {
  std::vector<IROperand> arg_ops;
  for (const auto &arg : node.args) {
    if (arg) {
      arg_ops.push_back(lower_expression(*arg));
    }
  }

  IROperand dst_op = new_var();
  IRInstruction call_inst;
  call_inst.op = IROpKind::Call;
  call_inst.dst = dst_op;
  call_inst.src1 = IROperand::global(node.callee);
  call_inst.args = std::move(arg_ops);

  emit_instruction(call_inst);
  last_expr_operand_ = dst_op;
}

void IRBuilder::visit(const IfNode &node) {
  std::string then_lbl = new_label("if_then");
  std::string else_lbl = new_label("if_else");
  std::string end_lbl = new_label("if_end");

  IROperand cond_op = lower_expression(*node.cond);

  IRInstruction br;
  br.op = IROpKind::BranchIf;
  br.src1 = cond_op;
  br.dst = IROperand::label(then_lbl);
  br.src2 = IROperand::label(node.else_branch ? else_lbl : end_lbl);
  emit_instruction(br);

  // Then branch
  IRInstruction lbl_then;
  lbl_then.op = IROpKind::Label;
  lbl_then.dst = IROperand::label(then_lbl);
  emit_instruction(lbl_then);

  if (node.then_branch) {
    node.then_branch->accept(*this);
  }

  if (node.else_branch) {
    IRInstruction jmp_end;
    jmp_end.op = IROpKind::Jump;
    jmp_end.dst = IROperand::label(end_lbl);
    emit_instruction(jmp_end);

    // Else branch
    IRInstruction lbl_else;
    lbl_else.op = IROpKind::Label;
    lbl_else.dst = IROperand::label(else_lbl);
    emit_instruction(lbl_else);

    node.else_branch->accept(*this);
  }

  // End label
  IRInstruction lbl_end;
  lbl_end.op = IROpKind::Label;
  lbl_end.dst = IROperand::label(end_lbl);
  emit_instruction(lbl_end);
}

void IRBuilder::visit(const WhileNode &node) {
  std::string loop_start = new_label("while_start");
  std::string loop_body = new_label("while_body");
  std::string loop_end = new_label("while_end");

  break_labels_.push_back(loop_end);
  continue_labels_.push_back(loop_start);

  // Start label
  IRInstruction lbl_start;
  lbl_start.op = IROpKind::Label;
  lbl_start.dst = IROperand::label(loop_start);
  emit_instruction(lbl_start);

  IROperand cond_op = lower_expression(*node.cond);

  IRInstruction br;
  br.op = IROpKind::BranchIf;
  br.src1 = cond_op;
  br.dst = IROperand::label(loop_body);
  br.src2 = IROperand::label(loop_end);
  emit_instruction(br);

  // Body
  IRInstruction lbl_body;
  lbl_body.op = IROpKind::Label;
  lbl_body.dst = IROperand::label(loop_body);
  emit_instruction(lbl_body);

  if (node.body) {
    node.body->accept(*this);
  }

  IRInstruction jmp_start;
  jmp_start.op = IROpKind::Jump;
  jmp_start.dst = IROperand::label(loop_start);
  emit_instruction(jmp_start);

  // End
  IRInstruction lbl_end;
  lbl_end.op = IROpKind::Label;
  lbl_end.dst = IROperand::label(loop_end);
  emit_instruction(lbl_end);

  break_labels_.pop_back();
  continue_labels_.pop_back();
}

void IRBuilder::visit(const ForNode &node) {
  if (node.init) {
    node.init->accept(*this);
  }

  std::string loop_cond = new_label("for_cond");
  std::string loop_body = new_label("for_body");
  std::string loop_step = new_label("for_step");
  std::string loop_end = new_label("for_end");

  break_labels_.push_back(loop_end);
  continue_labels_.push_back(loop_step);

  // Condition label
  IRInstruction lbl_cond;
  lbl_cond.op = IROpKind::Label;
  lbl_cond.dst = IROperand::label(loop_cond);
  emit_instruction(lbl_cond);

  if (node.cond) {
    IROperand cond_op = lower_expression(*node.cond);
    IRInstruction br;
    br.op = IROpKind::BranchIf;
    br.src1 = cond_op;
    br.dst = IROperand::label(loop_body);
    br.src2 = IROperand::label(loop_end);
    emit_instruction(br);
  }

  // Body
  IRInstruction lbl_body;
  lbl_body.op = IROpKind::Label;
  lbl_body.dst = IROperand::label(loop_body);
  emit_instruction(lbl_body);

  if (node.body) {
    node.body->accept(*this);
  }

  // Step
  IRInstruction lbl_step;
  lbl_step.op = IROpKind::Label;
  lbl_step.dst = IROperand::label(loop_step);
  emit_instruction(lbl_step);

  if (node.step) {
    lower_expression(*node.step);
  }

  IRInstruction jmp_cond;
  jmp_cond.op = IROpKind::Jump;
  jmp_cond.dst = IROperand::label(loop_cond);
  emit_instruction(jmp_cond);

  // End
  IRInstruction lbl_end;
  lbl_end.op = IROpKind::Label;
  lbl_end.dst = IROperand::label(loop_end);
  emit_instruction(lbl_end);

  break_labels_.pop_back();
  continue_labels_.pop_back();
}

void IRBuilder::visit(const DoWhileNode &node) {
  std::string loop_body = new_label("dowhile_body");
  std::string loop_cond = new_label("dowhile_cond");
  std::string loop_end = new_label("dowhile_end");

  break_labels_.push_back(loop_end);
  continue_labels_.push_back(loop_cond);

  IRInstruction lbl_body;
  lbl_body.op = IROpKind::Label;
  lbl_body.dst = IROperand::label(loop_body);
  emit_instruction(lbl_body);

  if (node.body) {
    node.body->accept(*this);
  }

  IRInstruction lbl_cond;
  lbl_cond.op = IROpKind::Label;
  lbl_cond.dst = IROperand::label(loop_cond);
  emit_instruction(lbl_cond);

  IROperand cond_op = lower_expression(*node.cond);
  IRInstruction br;
  br.op = IROpKind::BranchIf;
  br.src1 = cond_op;
  br.dst = IROperand::label(loop_body);
  br.src2 = IROperand::label(loop_end);
  emit_instruction(br);

  IRInstruction lbl_end;
  lbl_end.op = IROpKind::Label;
  lbl_end.dst = IROperand::label(loop_end);
  emit_instruction(lbl_end);

  break_labels_.pop_back();
  continue_labels_.pop_back();
}

void IRBuilder::visit(const SwitchNode &node) {
  IROperand cond_op = lower_expression(*node.cond);
  std::string switch_end = new_label("switch_end");
  break_labels_.push_back(switch_end);

  std::vector<std::string> case_labels;
  std::string default_label = switch_end;

  for (size_t i = 0; i < node.cases.size(); ++i) {
    if (node.cases[i]->is_default) {
      default_label = new_label("case_default");
      case_labels.push_back(default_label);
    } else {
      case_labels.push_back(new_label("case"));
    }
  }

  // Branch checks
  for (size_t i = 0; i < node.cases.size(); ++i) {
    if (!node.cases[i]->is_default && node.cases[i]->value) {
      IROperand case_val = lower_expression(*node.cases[i]->value);
      IROperand cmp_op = new_var();
      IRInstruction eq_inst;
      eq_inst.op = IROpKind::Eq;
      eq_inst.dst = cmp_op;
      eq_inst.src1 = cond_op;
      eq_inst.src2 = case_val;
      emit_instruction(eq_inst);

      std::string next_check = (i + 1 < node.cases.size()) ? new_label("switch_check") : default_label;
      IRInstruction br;
      br.op = IROpKind::BranchIf;
      br.src1 = cmp_op;
      br.dst = IROperand::label(case_labels[i]);
      br.src2 = IROperand::label(next_check);
      emit_instruction(br);

      if (i + 1 < node.cases.size()) {
        IRInstruction lbl_chk;
        lbl_chk.op = IROpKind::Label;
        lbl_chk.dst = IROperand::label(next_check);
        emit_instruction(lbl_chk);
      }
    }
  }

  IRInstruction jmp_def;
  jmp_def.op = IROpKind::Jump;
  jmp_def.dst = IROperand::label(default_label);
  emit_instruction(jmp_def);

  // Case bodies
  for (size_t i = 0; i < node.cases.size(); ++i) {
    IRInstruction lbl_c;
    lbl_c.op = IROpKind::Label;
    lbl_c.dst = IROperand::label(case_labels[i]);
    emit_instruction(lbl_c);

    for (const auto &stmt : node.cases[i]->statements) {
      if (stmt) stmt->accept(*this);
    }
  }

  IRInstruction lbl_end;
  lbl_end.op = IROpKind::Label;
  lbl_end.dst = IROperand::label(switch_end);
  emit_instruction(lbl_end);

  break_labels_.pop_back();
}

void IRBuilder::visit(const CaseNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) stmt->accept(*this);
  }
}

void IRBuilder::visit(const BreakNode &/*node*/) {
  if (!break_labels_.empty()) {
    IRInstruction jmp;
    jmp.op = IROpKind::Jump;
    jmp.dst = IROperand::label(break_labels_.back());
    emit_instruction(jmp);
  }
}

void IRBuilder::visit(const ContinueNode &/*node*/) {
  if (!continue_labels_.empty()) {
    IRInstruction jmp;
    jmp.op = IROpKind::Jump;
    jmp.dst = IROperand::label(continue_labels_.back());
    emit_instruction(jmp);
  }
}

void IRBuilder::visit(const GotoNode &node) {
  IRInstruction jmp;
  jmp.op = IROpKind::Jump;
  jmp.dst = IROperand::label(node.label);
  emit_instruction(jmp);
}

void IRBuilder::visit(const LabelNode &node) {
  IRInstruction lbl;
  lbl.op = IROpKind::Label;
  lbl.dst = IROperand::label(node.name);
  emit_instruction(lbl);
}

void IRBuilder::visit(const ArrayAccessNode &node) {
  IROperand arr_op = lower_expression(*node.array);
  IROperand idx_op = lower_expression(*node.index);
  IROperand gep_op = new_var();

  IRInstruction gep;
  gep.op = IROpKind::GetElementPtr;
  gep.dst = gep_op;
  gep.src1 = arr_op;
  gep.src2 = idx_op;
  emit_instruction(gep);

  IROperand val_op = new_var();
  IRInstruction load;
  load.op = IROpKind::Load;
  load.dst = val_op;
  load.src1 = gep_op;
  emit_instruction(load);

  last_expr_operand_ = val_op;
}

void IRBuilder::visit(const InitListNode &/*node*/) {
  last_expr_operand_ = IROperand::none();
}

void IRBuilder::visit(const StructDefNode &node) {
  if (node.var_decl) {
    node.var_decl->accept(*this);
  }
}

void IRBuilder::visit(const MemberAccessNode &node) {
  IROperand obj_op = lower_expression(*node.object);
  IROperand gep_op = new_var();

  IRInstruction gep;
  gep.op = IROpKind::GetElementPtr;
  gep.dst = gep_op;
  gep.src1 = obj_op;
  gep.src2 = IROperand::global(node.member);
  emit_instruction(gep);

  IROperand val_op = new_var();
  IRInstruction load;
  load.op = IROpKind::Load;
  load.dst = val_op;
  load.src1 = gep_op;
  emit_instruction(load);

  last_expr_operand_ = val_op;
}

void IRBuilder::visit(const AsmStmtNode &/*node*/) {
  // Pass-through or nop in target-independent IR
  IRInstruction nop;
  nop.op = IROpKind::Nop;
  nop.comment = "inline asm";
  emit_instruction(nop);
}
