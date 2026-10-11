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

int AVRCodeGenerator::get_type_size(DataType type, const std::string &st_name) const {
  if (type == DataType::Struct || type == DataType::Union) {
    auto it = struct_defs_.find(st_name);
    if (it != struct_defs_.end()) {
      return it->second.total_size > 0 ? it->second.total_size : 2;
    }
    return 2;
  }
  return 2;
}

std::string AVRCodeGenerator::get_expr_struct_type(const ASTNode *expr) const {
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
      size += v->array_size * 2;
    } else {
      size += 2;
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
    size += 2;
    for (const auto &c : s->cases) {
      for (const auto &st : c->statements) {
        size += compute_ast_frame_bytes(st.get());
      }
    }
  }
  return size;
}

void AVRCodeGenerator::generate(const ProgramNode &program, std::ostream &out) {
  out_ = &out;
  var_offsets_.clear();
  is_array_.clear();
  array_sizes_.clear();
  is_func_ptr_.clear();
  var_struct_name_.clear();
  struct_defs_.clear();
  current_offset_ = 0;
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

  emit_raw("; ================================================================");
  emit_raw("; AVR Assembly generated for ATmega328P (Arduino UNO)");
  emit_raw("; ABI: 16-bit ints, r24:r25 accumulator/return, Y (r28:r29) frame");
  emit_raw("; ================================================================");
  emit_raw(".arch atmega328p");

  if (!has_func_defs) {
    int extra = 0;
    for (const auto &stmt : program.statements) {
      extra += compute_ast_frame_bytes(stmt.get());
    }
    int frame_size = (extra + 2 > 32) ? (extra + 2) : 32;

    ret_label_ = ".Lmain_avr_ret";
    emit_raw(".global main");
    emit_raw("main:");

    // Prologue: Save Y frame pointer and set up stack frame
    emit_line("push r28");
    emit_line("push r29");
    emit_line("in r28, 0x3d        ; SPL");
    emit_line("in r29, 0x3e        ; SPH");
    emit_line("sbiw r28, " + std::to_string(frame_size) + "        ; allocate local stack frame");
    emit_line("in r0, 0x3f         ; SREG");
    emit_line("cli");
    emit_line("out 0x3e, r29       ; update SPH");
    emit_line("out 0x3f, r0        ; restore SREG");
    emit_line("out 0x3d, r28       ; update SPL");

    program.accept(*this);

    // Epilogue: Restore stack and frame pointer, then return
    emit_raw(ret_label_ + ":");
    emit_line("adiw r28, " + std::to_string(frame_size) + "        ; free stack frame");
    emit_line("in r0, 0x3f");
    emit_line("cli");
    emit_line("out 0x3e, r29");
    emit_line("out 0x3f, r0");
    emit_line("out 0x3d, r28");
    emit_line("pop r29");
    emit_line("pop r28");
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
    is_array_.clear();
    array_sizes_.clear();
    is_func_ptr_.clear();
    current_offset_ = 0;

    int extra = 0;
    for (const auto *stmt : non_func_stmts) {
      extra += compute_ast_frame_bytes(stmt);
    }
    int frame_size = (extra + 2 > 32) ? (extra + 2) : 32;

    ret_label_ = ".Lmain_avr_ret";
    emit_raw(".global main");
    emit_raw("main:");

    emit_line("push r28");
    emit_line("push r29");
    emit_line("in r28, 0x3d        ; SPL");
    emit_line("in r29, 0x3e        ; SPH");
    emit_line("sbiw r28, " + std::to_string(frame_size) + "        ; allocate local stack frame");
    emit_line("in r0, 0x3f         ; SREG");
    emit_line("cli");
    emit_line("out 0x3e, r29       ; update SPH");
    emit_line("out 0x3f, r0        ; restore SREG");
    emit_line("out 0x3d, r28       ; update SPL");

    for (const auto *stmt : non_func_stmts) {
      stmt->accept(*this);
    }

    emit_raw(ret_label_ + ":");
    emit_line("adiw r28, " + std::to_string(frame_size) + "        ; free stack frame");
    emit_line("in r0, 0x3f");
    emit_line("cli");
    emit_line("out 0x3e, r29");
    emit_line("out 0x3f, r0");
    emit_line("out 0x3d, r28");
    emit_line("pop r29");
    emit_line("pop r28");
    emit_line("ret");
  }
}

void AVRCodeGenerator::visit(const FuncDefNode &node) {
  if (!node.body) {
    return;
  }
  var_offsets_.clear();
  is_array_.clear();
  array_sizes_.clear();
  is_func_ptr_.clear();
  current_offset_ = 0;
  break_stack_.clear();
  continue_stack_.clear();

  std::string prev_ret_label = ret_label_;
  ret_label_ = ".L" + node.name + "_avr_ret";

  int extra = static_cast<int>(node.params.size()) * 2 +
              compute_ast_frame_bytes(node.body.get());
  int frame_size = (extra + 2 > 32) ? (extra + 2) : 32;

  emit_raw(".global " + node.name);
  emit_raw(node.name + ":");

  // Prologue: Save Y frame pointer and set up stack frame
  emit_line("push r28");
  emit_line("push r29");
  emit_line("in r28, 0x3d        ; SPL");
  emit_line("in r29, 0x3e        ; SPH");
  emit_line("sbiw r28, " + std::to_string(frame_size) + "        ; allocate local stack frame");
  emit_line("in r0, 0x3f         ; SREG");
  emit_line("cli");
  emit_line("out 0x3e, r29       ; update SPH");
  emit_line("out 0x3f, r0        ; restore SREG");
  emit_line("out 0x3d, r28       ; update SPL");

  static const std::pair<const char *, const char *> ARG_REGS[] = {
      {"r24", "r25"}, {"r22", "r23"}, {"r20", "r21"}, {"r18", "r19"}};

  for (size_t i = 0; i < node.params.size(); ++i) {
    if (i < 4) {
      const auto &p = node.params[i];
      if (p.type == DataType::Long || p.type == DataType::LongLong) {
        unsupported(node, "type modifier 'long' on AVR parameter");
      }
      if (p.is_func_ptr) {
        is_func_ptr_[p.name] = true;
      }
      int offset = get_var_offset(p.name);
      emit_line("std Y+" + std::to_string(offset - 1) + ", " + ARG_REGS[i].first);
      emit_line("std Y+" + std::to_string(offset) + ", " +
                ARG_REGS[i].second);
    } else {
      unsupported(node, "more than 4 function parameters on AVR");
    }
  }

  if (node.body) {
    node.body->accept(*this);
  }

  // Epilogue: Restore stack and frame pointer, then return
  emit_raw(ret_label_ + ":");
  emit_line("adiw r28, " + std::to_string(frame_size) + "        ; free stack frame");
  emit_line("in r0, 0x3f");
  emit_line("cli");
  emit_line("out 0x3e, r29");
  emit_line("out 0x3f, r0");
  emit_line("out 0x3d, r28");
  emit_line("pop r29");
  emit_line("pop r28");
  emit_line("ret");

  ret_label_ = prev_ret_label;
}

void AVRCodeGenerator::visit(const CallNode &node) {
  if (node.args.size() > 4) {
    unsupported(node, "more than 4 function call arguments on AVR");
  }

  bool is_indirect = false;
  if (node.callee_expr) {
    is_indirect = true;
    node.callee_expr->accept(*this);
    emit_line("push r24");
    emit_line("push r25");
  } else if (!node.callee.empty() && var_offsets_.find(node.callee) != var_offsets_.end()) {
    is_indirect = true;
    int offset = get_var_offset(node.callee);
    emit_line("ldd r24, Y+" + std::to_string(offset - 1));
    emit_line("ldd r25, Y+" + std::to_string(offset));
    emit_line("push r24");
    emit_line("push r25");
  }

  for (const auto &arg : node.args) {
    if (arg) {
      arg->accept(*this);
    } else {
      emit_line("ldi r24, 0");
      emit_line("ldi r25, 0");
    }
    emit_line("push r24");
    emit_line("push r25");
  }

  static const std::pair<const char *, const char *> ARG_REGS[] = {
      {"r24", "r25"}, {"r22", "r23"}, {"r20", "r21"}, {"r18", "r19"}};

  for (int i = static_cast<int>(node.args.size()) - 1; i >= 0; --i) {
    emit_line(std::string("pop ") + ARG_REGS[i].second);
    emit_line(std::string("pop ") + ARG_REGS[i].first);
  }

  if (is_indirect) {
    emit_line("pop r31");
    emit_line("pop r30");
    emit_line("icall");
  } else {
    emit_line("call " + node.callee);
  }
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
  if (node.data_type == DataType::Long || node.data_type == DataType::LongLong) {
    unsupported(node, "type modifier 'long'");
  }
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
      emit_line("std Y+" + std::to_string(offset - 1) + ", r24");
      emit_line("std Y+" + std::to_string(offset) + ", r25");
    }
    return;
  }
  if (node.is_array) {
    is_array_[node.name] = true;
    array_sizes_[node.name] = node.array_size;
    int elem_size = 2;
    if (node.data_type == DataType::Struct || node.data_type == DataType::Union) {
      elem_size = get_type_size(node.data_type, node.struct_name);
    }
    int base_offset = current_offset_ + 1;
    current_offset_ += node.array_size * elem_size;
    var_offsets_[node.name] = base_offset;

    if (node.init) {
      const auto *init_list = static_cast<const InitListNode *>(node.init.get());
      for (size_t i = 0; i < init_list->elements.size(); ++i) {
        if (init_list->elements[i]) {
          init_list->elements[i]->accept(*this);
          int elem_offset = base_offset + static_cast<int>(i) * elem_size;
          emit_line("std Y+" + std::to_string(elem_offset) + ", r24");
          emit_line("std Y+" + std::to_string(elem_offset + 1) + ", r25");
        }
      }
    }
  } else {
    int st_size = get_type_size(node.data_type, node.struct_name);
    int base_offset = current_offset_ + 1;
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
            emit_line("std Y+" + std::to_string(elem_offset) + ", r24");
            emit_line("std Y+" + std::to_string(elem_offset + 1) + ", r25");
          }
        }
      } else {
        node.init->accept(*this);
        emit_line("std Y+" + std::to_string(base_offset) + ", r24");
        emit_line("std Y+" + std::to_string(base_offset + 1) + ", r25");
      }
    }
  }
}

void AVRCodeGenerator::visit(const ArrayAccessNode &node) {
  if (!node.array || node.array->getType() != ASTNodeType::Identifier) {
    unsupported(node, "complex array access");
  }
  auto *ident = static_cast<const IdentifierNode *>(node.array.get());
  int base_offset = get_var_offset(ident->name);

  if (node.index) {
    node.index->accept(*this);
  } else {
    emit_line("ldi r24, 0");
    emit_line("ldi r25, 0");
  }

  emit_line("lsl r24");
  emit_line("rol r25");
  emit_line("subi r24, lo8(-" + std::to_string(base_offset) + ")");
  emit_line("sbci r25, hi8(-" + std::to_string(base_offset) + ")");
  emit_line("movw r30, r28");
  emit_line("add r30, r24");
  emit_line("adc r31, r25");
  emit_line("ld r24, Z");
  emit_line("ldd r25, Z+1");
}

void AVRCodeGenerator::visit(const InitListNode &node) {
  for (const auto &elem : node.elements) {
    if (elem) {
      elem->accept(*this);
    }
  }
}

void AVRCodeGenerator::visit(const AssignNode &node) {
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
      emit_line("ldi r24, 0");
      emit_line("ldi r25, 0");
    }
    emit_line("push r24");
    emit_line("push r25");

    if (node.right) {
      node.right->accept(*this);
    }

    emit_line("pop r19");
    emit_line("pop r18");
    emit_line("lsl r18");
    emit_line("rol r19");
    emit_line("subi r18, lo8(-" + std::to_string(base_offset) + ")");
    emit_line("sbci r19, hi8(-" + std::to_string(base_offset) + ")");
    emit_line("movw r30, r28");
    emit_line("add r30, r18");
    emit_line("adc r31, r19");
    emit_line("st Z, r24");
    emit_line("std Z+1, r25");
    return;
  }

  if (node.left->getType() == ASTNodeType::UnaryOp &&
      static_cast<const UnaryOpNode *>(node.left.get())->op == TOK_STAR) {
    auto *unary = static_cast<const UnaryOpNode *>(node.left.get());
    if (unary->expr) {
      unary->expr->accept(*this);
    } else {
      emit_line("ldi r24, 0");
      emit_line("ldi r25, 0");
    }
    emit_line("push r24");
    emit_line("push r25");

    if (node.right) {
      node.right->accept(*this);
    }

    emit_line("pop r31");
    emit_line("pop r30");
    emit_line("st Z, r24");
    emit_line("std Z+1, r25");
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
      emit_line("push r24");
      emit_line("push r25");
      mem->object->accept(*this);
      emit_line("movw r30, r24");
      if (m.offset > 0) {
        emit_line("adiw r30, " + std::to_string(m.offset));
      }
      emit_line("pop r25");
      emit_line("pop r24");
      emit_line("st Z, r24");
      emit_line("std Z+1, r25");
      return;
    } else {
      if (mem->object->getType() == ASTNodeType::Identifier) {
        auto *ident = static_cast<const IdentifierNode *>(mem->object.get());
        int base_offset = get_var_offset(ident->name);
        int mem_off = base_offset + m.offset;
        if (node.right) {
          node.right->accept(*this);
        }
        emit_line("std Y+" + std::to_string(mem_off) + ", r24");
        emit_line("std Y+" + std::to_string(mem_off + 1) + ", r25");
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
  auto it_offset = var_offsets_.find(node.name);
  if (it_offset == var_offsets_.end()) {
    // Function address decay
    emit_line("ldi r24, pm_lo8(" + node.name + ")");
    emit_line("ldi r25, pm_hi8(" + node.name + ")");
    return;
  }
  int offset = it_offset->second;
  auto it = is_array_.find(node.name);
  if (it != is_array_.end() && it->second) {
    emit_line("movw r24, r28");
    emit_line("subi r24, lo8(-" + std::to_string(offset) + ")");
    emit_line("sbci r25, hi8(-" + std::to_string(offset) + ")");
    return;
  }
  emit_line("ldd r24, Y+" + std::to_string(offset - 1));
  emit_line("ldd r25, Y+" + std::to_string(offset));
}

void AVRCodeGenerator::visit(const UnaryOpNode &node) {
  if (node.op == TOK_AMP) {
    if (!node.expr) {
      unsupported(node, "empty address-of");
    }
    if (node.expr->getType() == ASTNodeType::Identifier) {
      auto *ident = static_cast<const IdentifierNode *>(node.expr.get());
      auto it = var_offsets_.find(ident->name);
      if (it != var_offsets_.end()) {
        int offset = it->second;
        emit_line("movw r24, r28");
        emit_line("subi r24, lo8(-" + std::to_string(offset) + ")");
        emit_line("sbci r25, hi8(-" + std::to_string(offset) + ")");
      } else {
        emit_line("ldi r24, pm_lo8(" + ident->name + ")");
        emit_line("ldi r25, pm_hi8(" + ident->name + ")");
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
        emit_line("ldi r24, 0");
        emit_line("ldi r25, 0");
      }
      emit_line("lsl r24");
      emit_line("rol r25");
      emit_line("subi r24, lo8(-" + std::to_string(base_offset) + ")");
      emit_line("sbci r25, hi8(-" + std::to_string(base_offset) + ")");
      emit_line("movw r30, r28");
      emit_line("add r30, r24");
      emit_line("adc r31, r25");
      emit_line("movw r24, r30");
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
          emit_line("movw r30, r24");
          emit_line("adiw r30, " + std::to_string(m.offset));
          emit_line("movw r24, r30");
        }
        return;
      } else {
        if (mem->object->getType() == ASTNodeType::Identifier) {
          auto *ident = static_cast<const IdentifierNode *>(mem->object.get());
          int base_offset = get_var_offset(ident->name);
          int mem_off = base_offset + m.offset;
          emit_line("movw r24, r28");
          emit_line("subi r24, lo8(-" + std::to_string(mem_off) + ")");
          emit_line("sbci r25, hi8(-" + std::to_string(mem_off) + ")");
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
      emit_line("ldi r24, 0");
      emit_line("ldi r25, 0");
    }
    emit_line("movw r30, r24");
    emit_line("ld r24, Z");
    emit_line("ldd r25, Z+1");
    return;
  }

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

  break_stack_.push_back(end_lbl);
  continue_stack_.push_back(cond_lbl);
  if (node.body) {
    node.body->accept(*this);
  }
  continue_stack_.pop_back();
  break_stack_.pop_back();

  emit_line("rjmp " + cond_lbl);
  emit_raw(end_lbl + ":");
}

void AVRCodeGenerator::visit(const DoWhileNode &node) {
  std::string body_lbl = new_label(".Lavr_dowhile_body");
  std::string cond_lbl = new_label(".Lavr_dowhile_cond");
  std::string end_lbl = new_label(".Lavr_dowhile_end");

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
    emit_line("sbiw r24, 0");
    emit_line("brne " + body_lbl);
  }
  break_stack_.pop_back();
  emit_raw(end_lbl + ":");
}

void AVRCodeGenerator::visit(const SwitchNode &node) {
  std::string end_lbl = new_label(".Lavr_switch_end");
  std::string default_lbl = "";
  int cond_offset = get_var_offset("@avr_switch_cond_" + std::to_string(label_counter_));

  // 1. Evaluate switch condition into r24:r25 and save in frame slot
  if (node.cond) {
    node.cond->accept(*this);
  }
  emit_line("std Y+" + std::to_string(cond_offset - 1) + ", r24");
  emit_line("std Y+" + std::to_string(cond_offset) + ", r25");

  // 2. Generate case labels and comparison branches
  std::vector<std::string> case_labels;
  case_labels.reserve(node.cases.size());

  for (size_t i = 0; i < node.cases.size(); ++i) {
    const auto &c = node.cases[i];
    if (c->is_default) {
      std::string d_lbl = new_label(".Lavr_switch_default");
      default_lbl = d_lbl;
      case_labels.push_back(d_lbl);
    } else {
      std::string c_lbl = new_label(".Lavr_switch_case");
      case_labels.push_back(c_lbl);

      if (c->value) {
        c->value->accept(*this);
      }
      emit_line("ldd r18, Y+" + std::to_string(cond_offset - 1));
      emit_line("ldd r19, Y+" + std::to_string(cond_offset));
      emit_line("cp r18, r24");
      emit_line("cpc r19, r25");
      emit_line("breq " + c_lbl);
    }
  }

  // 3. Fallthrough to default or end
  if (!default_lbl.empty()) {
    emit_line("rjmp " + default_lbl);
  } else {
    emit_line("rjmp " + end_lbl);
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

void AVRCodeGenerator::visit(const CaseNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      stmt->accept(*this);
    }
  }
}

void AVRCodeGenerator::visit(const BreakNode &node) {
  if (break_stack_.empty()) {
    unsupported(node, "break statement outside of loop or switch");
  }
  emit_line("rjmp " + break_stack_.back());
}

void AVRCodeGenerator::visit(const ContinueNode &node) {
  if (continue_stack_.empty()) {
    unsupported(node, "continue statement outside of loop");
  }
  emit_line("rjmp " + continue_stack_.back());
}

void AVRCodeGenerator::visit(const GotoNode &node) {
  emit_line("rjmp .Luser_" + node.label);
}

void AVRCodeGenerator::visit(const LabelNode &node) {
  emit_raw(".Luser_" + node.name + ":");
}

void AVRCodeGenerator::visit(const ReturnNode &node) {
  if (node.expr) {
    node.expr->accept(*this);
  }
  emit_line("rjmp " + ret_label_);
}

void AVRCodeGenerator::visit(const StructDefNode &node) {
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
    int elem_size = 2;
    mi.size = m.is_array ? (m.array_size * elem_size) : elem_size;
    if (node.is_union) {
      mi.offset = 0;
      if (mi.size > max_size) max_size = mi.size;
    } else {
      mi.offset = offset;
      offset += mi.size;
    }
    info.members.push_back(mi);
    info.member_map[mi.name] = mi;
  }
  if (node.is_union) {
    info.total_size = max_size > 0 ? max_size : 2;
  } else {
    info.total_size = offset > 0 ? offset : 2;
  }
  if (!node.name.empty()) {
    struct_defs_[node.name] = info;
  }
  if (node.var_decl) {
    node.var_decl->accept(*this);
  }
}

void AVRCodeGenerator::visit(const MemberAccessNode &node) {
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
    emit_line("movw r30, r24");
    if (m.offset > 0) {
      emit_line("adiw r30, " + std::to_string(m.offset));
    }
    emit_line("ld r24, Z");
    emit_line("ldd r25, Z+1");
  } else {
    if (node.object->getType() == ASTNodeType::Identifier) {
      auto *ident = static_cast<const IdentifierNode *>(node.object.get());
      int base_offset = get_var_offset(ident->name);
      int mem_off = base_offset + m.offset;
      emit_line("ldd r24, Y+" + std::to_string(mem_off));
      emit_line("ldd r25, Y+" + std::to_string(mem_off + 1));
    } else {
      unsupported(node, "complex member access");
    }
  }
}
