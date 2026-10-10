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

DataType ARM64CodeGenerator::get_var_type(const std::string &name) const {
  auto it = var_types_.find(name);
  if (it != var_types_.end()) {
    return it->second;
  }
  return DataType::Int;
}

int ARM64CodeGenerator::get_type_size(DataType type, const std::string &st_name) const {
  if (type == DataType::Struct || type == DataType::Union) {
    auto it = struct_defs_.find(st_name);
    if (it != struct_defs_.end()) {
      return it->second.total_size > 0 ? it->second.total_size : 8;
    }
    return 8;
  }
  if (type == DataType::Short) return 2;
  return 8;
}

std::string ARM64CodeGenerator::get_expr_struct_type(const ASTNode *expr) const {
  if (!expr) return "";
  if (expr->getType() == ASTNodeType::Identifier) {
    auto it = var_struct_name_.find(static_cast<const IdentifierNode *>(expr)->name);
    return it != var_struct_name_.end() ? it->second : "";
  }
  if (expr->getType() == ASTNodeType::MemberAccess) {
    const auto *mem = static_cast<const MemberAccessNode *>(expr);
    std::string parent_st = get_expr_struct_type(mem->object.get());
    auto it = struct_defs_.find(parent_st);
    if (it != struct_defs_.end()) {
      auto mit = it->second.member_map.find(mem->member);
      if (mit != it->second.member_map.end()) {
        return mit->second.struct_name;
      }
    }
    return "";
  }
  if (expr->getType() == ASTNodeType::UnaryOp) {
    return get_expr_struct_type(static_cast<const UnaryOpNode *>(expr)->expr.get());
  }
  if (expr->getType() == ASTNodeType::ArrayAccess) {
    return get_expr_struct_type(static_cast<const ArrayAccessNode *>(expr)->array.get());
  }
  return "";
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

void ARM64CodeGenerator::generate(const ProgramNode &program,
                                   std::ostream &out) {
  out_ = &out;
  var_offsets_.clear();
  var_types_.clear();
  is_array_.clear();
  array_sizes_.clear();
  is_func_ptr_.clear();
  var_struct_name_.clear();
  struct_defs_.clear();
  current_offset_ = 8; // offset 0 and 8 reserved for saved x29, x30
  label_counter_ = 0;
  break_stack_.clear();
  continue_stack_.clear();

  for (const auto &stmt : program.statements) {
    if (stmt && stmt->getType() == ASTNodeType::StructDef) {
      stmt->accept(*this);
    }
  }

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

  emit_raw("// ================================================================");
  emit_raw("// ARM64 (AArch64) Assembly for Raspberry Pi (ARMv8-A)");
  emit_raw("// ABI: AAPCS64, x0 return/accumulator, x29 frame pointer");
  emit_raw("// ================================================================");
  emit_raw(".arch armv8-a");
  emit_raw(".text");

  if (!has_func_defs) {
    int extra = 0;
    for (const auto &stmt : program.statements) {
      extra += compute_ast_frame_bytes(stmt.get());
    }
    int frame_size = (extra + 16 > 64) ? ((extra + 16 + 15) & ~15) : 64;

    ret_label_ = ".Lmain_arm_ret";
    emit_raw(".globl main");
    emit_raw(".type main, %function");
    emit_raw("main:");

    // Prologue: Allocate frame (16-byte aligned) & save frame pointer + link register
    emit_line("stp x29, x30, [sp, -" + std::to_string(frame_size) + "]!    // save FP and LR, alloc frame");
    emit_line("mov x29, sp                 // set frame pointer");

    program.accept(*this);

    // Epilogue: Restore frame and return
    emit_raw(ret_label_ + ":");
    emit_line("mov sp, x29                 // restore stack pointer");
    emit_line("ldp x29, x30, [sp], " + std::to_string(frame_size) + "      // restore FP and LR, dealloc frame");
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
    is_func_ptr_.clear();
    current_offset_ = 8;

    int extra = 0;
    for (const auto *stmt : non_func_stmts) {
      extra += compute_ast_frame_bytes(stmt);
    }
    int frame_size = (extra + 16 > 64) ? ((extra + 16 + 15) & ~15) : 64;

    ret_label_ = ".Lmain_arm_ret";
    emit_raw(".globl main");
    emit_raw(".type main, %function");
    emit_raw("main:");

    emit_line("stp x29, x30, [sp, -" + std::to_string(frame_size) + "]!    // save FP and LR, alloc frame");
    emit_line("mov x29, sp                 // set frame pointer");

    for (const auto *stmt : non_func_stmts) {
      stmt->accept(*this);
    }

    emit_raw(ret_label_ + ":");
    emit_line("mov sp, x29                 // restore stack pointer");
    emit_line("ldp x29, x30, [sp], " + std::to_string(frame_size) + "      // restore FP and LR, dealloc frame");
    emit_line("ret");
  }
}

void ARM64CodeGenerator::visit(const FuncDefNode &node) {
  var_offsets_.clear();
  var_types_.clear();
  is_array_.clear();
  array_sizes_.clear();
  is_func_ptr_.clear();
  current_offset_ = 8;
  break_stack_.clear();
  continue_stack_.clear();

  std::string prev_ret_label = ret_label_;
  ret_label_ = ".L" + node.name + "_arm_ret";

  int extra = static_cast<int>(node.params.size()) * 8 +
              compute_ast_frame_bytes(node.body.get());
  int frame_size = (extra + 16 > 64) ? ((extra + 16 + 15) & ~15) : 64;

  emit_raw(".globl " + node.name);
  emit_raw(".type " + node.name + ", %function");
  emit_raw(node.name + ":");
  emit_line("stp x29, x30, [sp, -" + std::to_string(frame_size) + "]!    // save FP and LR, alloc frame");
  emit_line("mov x29, sp                 // set frame pointer");

  static const char *const ARG_REGS[] = {
      "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};

  for (size_t i = 0; i < node.params.size(); ++i) {
    if (i < 8) {
      const auto &p = node.params[i];
      var_types_[p.name] = p.type;
      if (p.is_func_ptr) {
        is_func_ptr_[p.name] = true;
      }
      int offset = get_var_offset(p.name);
      emit_line("str " + std::string(ARG_REGS[i]) + ", [x29, #" +
                std::to_string(offset) + "]");
    } else {
      unsupported(node, "more than 8 function parameters");
    }
  }

  if (node.body) {
    node.body->accept(*this);
  }

  emit_raw(ret_label_ + ":");
  emit_line("mov sp, x29                 // restore stack pointer");
  emit_line("ldp x29, x30, [sp], " + std::to_string(frame_size) + "      // restore FP and LR, dealloc frame");
  emit_line("ret");

  ret_label_ = prev_ret_label;
}

void ARM64CodeGenerator::visit(const CallNode &node) {
  if (node.args.size() > 8) {
    unsupported(node, "more than 8 function call arguments");
  }

  bool is_indirect = false;
  if (node.callee_expr) {
    is_indirect = true;
    node.callee_expr->accept(*this);
    emit_line("str x0, [sp, -16]!");
  } else if (!node.callee.empty() && var_offsets_.find(node.callee) != var_offsets_.end()) {
    is_indirect = true;
    int offset = get_var_offset(node.callee);
    emit_line("ldr x0, [x29, #" + std::to_string(offset) + "]");
    emit_line("str x0, [sp, -16]!");
  }

  for (const auto &arg : node.args) {
    if (arg) {
      arg->accept(*this);
    } else {
      emit_line("mov x0, #0");
    }
    emit_line("str x0, [sp, -16]!");
  }

  static const char *const ARG_REGS[] = {
      "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};
  for (int i = static_cast<int>(node.args.size()) - 1; i >= 0; --i) {
    emit_line("ldr " + std::string(ARG_REGS[i]) + ", [sp], 16");
  }

  if (is_indirect) {
    emit_line("ldr x16, [sp], 16");
    emit_line("blr x16");
  } else {
    emit_line("bl " + node.callee);
  }
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
  var_types_[node.name] = node.data_type;
  if (node.is_func_ptr) {
    is_func_ptr_[node.name] = true;
  }
  if (!node.struct_name.empty()) {
    var_struct_name_[node.name] = node.struct_name;
  }
  if (node.pointer_level > 0 || (node.data_type != DataType::Struct && node.data_type != DataType::Union && !node.is_array)) {
    int offset = get_var_offset(node.name);
    if (node.init) {
      node.init->accept(*this);
      if (node.data_type == DataType::Short && node.pointer_level == 0 && !node.is_func_ptr) {
        emit_line("strh w0, [x29, #" + std::to_string(offset) + "]");
      } else {
        emit_line("str x0, [x29, #" + std::to_string(offset) + "]");
      }
    }
    return;
  }
  if (node.is_array) {
    is_array_[node.name] = true;
    array_sizes_[node.name] = node.array_size;
    int elem_size = 8;
    if (node.data_type == DataType::Short && node.pointer_level == 0) elem_size = 2;
    else if (node.data_type == DataType::Struct || node.data_type == DataType::Union) {
      elem_size = get_type_size(node.data_type, node.struct_name);
    }
    int base_offset = current_offset_ + elem_size;
    current_offset_ += node.array_size * elem_size;
    var_offsets_[node.name] = base_offset;

    if (node.init) {
      const auto *init_list = static_cast<const InitListNode *>(node.init.get());
      for (size_t i = 0; i < init_list->elements.size(); ++i) {
        if (init_list->elements[i]) {
          init_list->elements[i]->accept(*this);
          int elem_offset = base_offset + static_cast<int>(i) * elem_size;
          if (elem_size == 2) {
            emit_line("strh w0, [x29, #" + std::to_string(elem_offset) + "]");
          } else {
            emit_line("str x0, [x29, #" + std::to_string(elem_offset) + "]");
          }
        }
      }
    }
  } else {
    int st_size = get_type_size(node.data_type, node.struct_name);
    int base_offset = current_offset_ + 8;
    current_offset_ += st_size;
    var_offsets_[node.name] = base_offset;
    if (node.init) {
      if (node.init->getType() == ASTNodeType::InitList) {
        const auto *init_list = static_cast<const InitListNode *>(node.init.get());
        const auto &st_info = struct_defs_[node.struct_name];
        for (size_t i = 0; i < init_list->elements.size() && i < st_info.members.size(); ++i) {
          if (init_list->elements[i]) {
            init_list->elements[i]->accept(*this);
            int elem_offset = base_offset + st_info.members[i].offset;
            if (st_info.members[i].type == DataType::Short && st_info.members[i].pointer_level == 0) {
              emit_line("strh w0, [x29, #" + std::to_string(elem_offset) + "]");
            } else {
              emit_line("str x0, [x29, #" + std::to_string(elem_offset) + "]");
            }
          }
        }
      } else {
        node.init->accept(*this);
        emit_line("str x0, [x29, #" + std::to_string(base_offset) + "]");
      }
    }
  }
}

void ARM64CodeGenerator::visit(const ArrayAccessNode &node) {
  if (!node.array || node.array->getType() != ASTNodeType::Identifier) {
    unsupported(node, "complex array access");
  }
  auto *ident = static_cast<const IdentifierNode *>(node.array.get());
  int base_offset = get_var_offset(ident->name);

  if (node.index) {
    node.index->accept(*this);
  } else {
    emit_line("mov x0, #0");
  }

  emit_line("add x1, x29, #" + std::to_string(base_offset));
  if (get_var_type(ident->name) == DataType::Short) {
    emit_line("lsl x2, x0, #1");
    emit_line("add x1, x1, x2");
    emit_line("ldrsh x0, [x1]");
  } else {
    emit_line("lsl x2, x0, #3");
    emit_line("add x1, x1, x2");
    emit_line("ldr x0, [x1]");
  }
}

void ARM64CodeGenerator::visit(const InitListNode &node) {
  for (const auto &elem : node.elements) {
    if (elem) {
      elem->accept(*this);
    }
  }
}

void ARM64CodeGenerator::visit(const AssignNode &node) {
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
      emit_line("mov x0, #0");
    }
    emit_line("str x0, [sp, -16]!");

    if (node.right) {
      node.right->accept(*this);
    }

    emit_line("ldr x1, [sp], 16");
    emit_line("add x2, x29, #" + std::to_string(base_offset));
    if (get_var_type(ident->name) == DataType::Short) {
      emit_line("lsl x3, x1, #1");
      emit_line("add x2, x2, x3");
      emit_line("strh w0, [x2]");
    } else {
      emit_line("lsl x3, x1, #3");
      emit_line("add x2, x2, x3");
      emit_line("str x0, [x2]");
    }
    return;
  }

  if (node.left->getType() == ASTNodeType::UnaryOp &&
      static_cast<const UnaryOpNode *>(node.left.get())->op == TOK_STAR) {
    auto *unary = static_cast<const UnaryOpNode *>(node.left.get());
    if (unary->expr) {
      unary->expr->accept(*this);
    } else {
      emit_line("mov x0, #0");
    }
    emit_line("str x0, [sp, -16]!");

    if (node.right) {
      node.right->accept(*this);
    }

    emit_line("ldr x1, [sp], 16");
    emit_line("str x0, [x1]");
    return;
  }

  if (node.left->getType() == ASTNodeType::MemberAccess) {
    auto *mem = static_cast<const MemberAccessNode *>(node.left.get());
    std::string st_name = get_expr_struct_type(mem->object.get());
    auto it = struct_defs_.find(st_name);
    if (it == struct_defs_.end()) {
      unsupported(node, "unknown struct type '" + st_name + "'");
    }
    auto mit = it->second.member_map.find(mem->member);
    if (mit == it->second.member_map.end()) {
      unsupported(node, "unknown member '" + mem->member + "' in struct '" + st_name + "'");
    }
    const auto &m = mit->second;

    if (mem->is_arrow) {
      if (node.right) {
        node.right->accept(*this);
      }
      emit_line("str x0, [sp, -16]!");
      mem->object->accept(*this);
      if (m.offset > 0) {
        emit_line("add x0, x0, #" + std::to_string(m.offset));
      }
      emit_line("mov x1, x0");
      emit_line("ldr x0, [sp], 16");
      if (m.type == DataType::Short && m.pointer_level == 0) {
        emit_line("strh w0, [x1]");
      } else {
        emit_line("str x0, [x1]");
      }
      return;
    } else {
      if (mem->object->getType() == ASTNodeType::Identifier) {
        auto *ident = static_cast<const IdentifierNode *>(mem->object.get());
        int base_offset = get_var_offset(ident->name);
        int mem_offset = base_offset + m.offset;
        if (node.right) {
          node.right->accept(*this);
        }
        if (m.type == DataType::Short && m.pointer_level == 0) {
          emit_line("strh w0, [x29, #" + std::to_string(mem_offset) + "]");
        } else {
          emit_line("str x0, [x29, #" + std::to_string(mem_offset) + "]");
        }
        return;
      }
      unsupported(node, "complex member assignment");
    }
  }

  if (node.left->getType() != ASTNodeType::Identifier) {
    throw std::runtime_error("Left-hand side of assignment must be variable, array element, or dereference");
  }
  auto *ident = static_cast<const IdentifierNode *>(node.left.get());
  int offset = get_var_offset(ident->name);

  if (node.right) {
    node.right->accept(*this);
  }
  if (get_var_type(ident->name) == DataType::Short && !is_func_ptr_[ident->name]) {
    emit_line("strh w0, [x29, #" + std::to_string(offset) + "]");
  } else {
    emit_line("str x0, [x29, #" + std::to_string(offset) + "]");
  }
}

void ARM64CodeGenerator::visit(const IntLitNode &node) {
  if (node.val >= 0 && node.val <= 65535) {
    emit_line("mov x0, #" + std::to_string(node.val));
  } else {
    emit_line("ldr x0, =" + std::to_string(node.val));
  }
}

void ARM64CodeGenerator::visit(const IdentifierNode &node) {
  auto it_offset = var_offsets_.find(node.name);
  if (it_offset == var_offsets_.end()) {
    // Function address decay / symbol reference
    emit_line("adrp x0, " + node.name);
    emit_line("add x0, x0, :lo12:" + node.name);
    return;
  }
  int offset = it_offset->second;
  auto it = is_array_.find(node.name);
  if (it != is_array_.end() && it->second) {
    emit_line("add x0, x29, #" + std::to_string(offset));
    return;
  }
  if (get_var_type(node.name) == DataType::Short && !is_func_ptr_[node.name]) {
    emit_line("ldrsh x0, [x29, #" + std::to_string(offset) + "]");
  } else {
    emit_line("ldr x0, [x29, #" + std::to_string(offset) + "]");
  }
}

void ARM64CodeGenerator::visit(const UnaryOpNode &node) {
  if (node.op == TOK_AMP) {
    if (!node.expr) {
      unsupported(node, "empty address-of");
    }
    if (node.expr->getType() == ASTNodeType::Identifier) {
      auto *ident = static_cast<const IdentifierNode *>(node.expr.get());
      auto it = var_offsets_.find(ident->name);
      if (it != var_offsets_.end()) {
        emit_line("add x0, x29, #" + std::to_string(it->second));
      } else {
        emit_line("adrp x0, " + ident->name);
        emit_line("add x0, x0, :lo12:" + ident->name);
      }
      return;
    }
    if (node.expr->getType() == ASTNodeType::ArrayAccess) {
      auto *acc = static_cast<const ArrayAccessNode *>(node.expr.get());
      if (!acc->array || acc->array->getType() != ASTNodeType::Identifier) {
        unsupported(node, "complex array address");
      }
      auto *ident = static_cast<const IdentifierNode *>(acc->array.get());
      int base_offset = get_var_offset(ident->name);
      if (acc->index) {
        acc->index->accept(*this);
      } else {
        emit_line("mov x0, #0");
      }
      emit_line("add x1, x29, #" + std::to_string(base_offset));
      if (get_var_type(ident->name) == DataType::Short) {
        emit_line("lsl x2, x0, #1");
      } else {
        emit_line("lsl x2, x0, #3");
      }
      emit_line("add x0, x1, x2");
      return;
    }
    if (node.expr->getType() == ASTNodeType::MemberAccess) {
      auto *mem = static_cast<const MemberAccessNode *>(node.expr.get());
      std::string st_name = get_expr_struct_type(mem->object.get());
      auto it = struct_defs_.find(st_name);
      if (it == struct_defs_.end()) {
        unsupported(node, "unknown struct type '" + st_name + "'");
      }
      const auto &m = it->second.member_map.at(mem->member);
      if (mem->is_arrow) {
        mem->object->accept(*this);
        if (m.offset > 0) {
          emit_line("add x0, x0, #" + std::to_string(m.offset));
        }
        return;
      } else {
        if (mem->object->getType() == ASTNodeType::Identifier) {
          auto *ident = static_cast<const IdentifierNode *>(mem->object.get());
          int base_offset = get_var_offset(ident->name);
          int mem_offset = base_offset + m.offset;
          emit_line("add x0, x29, #" + std::to_string(mem_offset));
          return;
        }
      }
    }
    unsupported(node, "unsupported address-of expression");
  }

  if (node.op == TOK_STAR) {
    if (node.expr) {
      node.expr->accept(*this);
    } else {
      emit_line("mov x0, #0");
    }
    emit_line("ldr x0, [x0]");
    return;
  }

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
  continue_stack_.push_back(cond_lbl);
  if (node.body) {
    node.body->accept(*this);
  }
  continue_stack_.pop_back();
  break_stack_.pop_back();

  emit_line("b " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void ARM64CodeGenerator::visit(const DoWhileNode &node) {
  std::string body_lbl = new_label(".Larm_dowhile_body");
  std::string cond_lbl = new_label(".Larm_dowhile_cond");
  std::string end_lbl = new_label(".Larm_dowhile_end");

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

void ARM64CodeGenerator::visit(const ContinueNode &node) {
  if (continue_stack_.empty()) {
    unsupported(node, "continue statement outside of loop");
  }
  emit_line("b " + continue_stack_.back());
}

void ARM64CodeGenerator::visit(const GotoNode &node) {
  emit_line("b .Luser_" + node.label);
}

void ARM64CodeGenerator::visit(const LabelNode &node) {
  emit_raw(".Luser_" + node.name + ":");
}

void ARM64CodeGenerator::visit(const ReturnNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  emit_line("b " + ret_label_);
}

void ARM64CodeGenerator::visit(const StructDefNode &node) {
  StructInfo info;
  info.name = node.name;
  info.is_union = node.is_union;
  int offset = 0;
  int max_size = 0;
  for (const auto &m : node.members) {
    MemberInfo mi;
    mi.name = m.name;
    mi.type = m.type;
    mi.pointer_level = m.pointer_level;
    mi.is_array = m.is_array;
    mi.array_size = m.array_size;
    mi.struct_name = m.struct_name;
    mi.is_union = m.is_union;
    int elem_size = (m.pointer_level > 0) ? 8 : get_type_size(m.type, m.struct_name);
    mi.size = m.is_array ? (m.array_size * elem_size) : elem_size;
    if (node.is_union) {
      mi.offset = 0;
      if (mi.size > max_size) max_size = mi.size;
    } else {
      mi.offset = offset;
      offset += mi.size;
      if (offset % 8 != 0) {
        offset += 8 - (offset % 8);
      }
    }
    info.members.push_back(mi);
    info.member_map[mi.name] = mi;
  }
  if (node.is_union) {
    if (max_size % 8 != 0) max_size += 8 - (max_size % 8);
    info.total_size = max_size > 0 ? max_size : 8;
  } else {
    info.total_size = offset > 0 ? offset : 8;
  }
  if (!node.name.empty()) {
    struct_defs_[node.name] = info;
  }
  if (node.var_decl) {
    node.var_decl->accept(*this);
  }
}

void ARM64CodeGenerator::visit(const MemberAccessNode &node) {
  std::string st_name = get_expr_struct_type(node.object.get());
  auto it = struct_defs_.find(st_name);
  if (it == struct_defs_.end()) {
    unsupported(node, "unknown struct type '" + st_name + "'");
  }
  auto mit = it->second.member_map.find(node.member);
  if (mit == it->second.member_map.end()) {
    unsupported(node, "unknown member '" + node.member + "' in struct '" + st_name + "'");
  }
  const auto &m = mit->second;

  if (node.is_arrow) {
    node.object->accept(*this);
    if (m.offset > 0) {
      emit_line("add x0, x0, #" + std::to_string(m.offset));
    }
    if (m.type == DataType::Short && m.pointer_level == 0) {
      emit_line("ldrsh x0, [x0]");
    } else {
      emit_line("ldr x0, [x0]");
    }
  } else {
    if (node.object->getType() == ASTNodeType::Identifier) {
      auto *ident = static_cast<const IdentifierNode *>(node.object.get());
      int base_offset = get_var_offset(ident->name);
      int mem_offset = base_offset + m.offset;
      if (m.type == DataType::Short && m.pointer_level == 0) {
        emit_line("ldrsh x0, [x29, #" + std::to_string(mem_offset) + "]");
      } else {
        emit_line("ldr x0, [x29, #" + std::to_string(mem_offset) + "]");
      }
    } else {
      unsupported(node, "complex member access");
    }
  }
}
