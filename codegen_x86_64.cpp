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

void X86_64CodeGenerator::emit_string_literal(const std::string &str) {
  *out_ << "    .string \"";
  for (unsigned char c : str) {
    if (c == '"') *out_ << "\\\"";
    else if (c == '\\') *out_ << "\\\\";
    else if (c == '\n') *out_ << "\\n";
    else if (c == '\r') *out_ << "\\r";
    else if (c == '\t') *out_ << "\\t";
    else if (c >= 32 && c <= 126) *out_ << static_cast<char>(c);
    else {
      *out_ << "\\" << std::oct << static_cast<int>(c) << std::dec;
    }
  }
  *out_ << "\"\n";
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

int X86_64CodeGenerator::get_var_pointer_level(const std::string &name) const {
  auto it = var_pointer_level_.find(name);
  if (it != var_pointer_level_.end()) {
    return it->second;
  }
  return 0;
}

bool X86_64CodeGenerator::is_func_ptr(const std::string &name) const {
  auto it = is_func_ptr_.find(name);
  return it != is_func_ptr_.end() && it->second;
}

DataType X86_64CodeGenerator::get_expr_type(const ASTNode *expr, int &pointer_level) const {
  pointer_level = 0;
  if (!expr) return DataType::Int;
  if (expr->getType() == ASTNodeType::Identifier) {
    const auto *id = static_cast<const IdentifierNode *>(expr);
    pointer_level = get_var_pointer_level(id->name);
    return get_var_type(id->name);
  }
  if (expr->getType() == ASTNodeType::MemberAccess) {
    const auto *mem = static_cast<const MemberAccessNode *>(expr);
    std::string parent_st = get_expr_struct_type(mem->object.get());
    auto it = struct_defs_.find(parent_st);
    if (it != struct_defs_.end()) {
      auto mit = it->second.member_map.find(mem->member);
      if (mit != it->second.member_map.end()) {
        pointer_level = mit->second.pointer_level;
        return mit->second.type;
      }
    }
    return DataType::Int;
  }
  if (expr->getType() == ASTNodeType::UnaryOp) {
    const auto *u = static_cast<const UnaryOpNode *>(expr);
    if (u->op == TOK_STAR) {
      int inner_ptr = 0;
      DataType t = get_expr_type(u->expr.get(), inner_ptr);
      pointer_level = inner_ptr > 0 ? inner_ptr - 1 : 0;
      return t;
    }
    if (u->op == TOK_AMP) {
      int inner_ptr = 0;
      DataType t = get_expr_type(u->expr.get(), inner_ptr);
      pointer_level = inner_ptr + 1;
      return t;
    }
  }
  if (expr->getType() == ASTNodeType::ArrayAccess) {
    const auto *arr = static_cast<const ArrayAccessNode *>(expr);
    int inner_ptr = 0;
    DataType t = get_expr_type(arr->array.get(), inner_ptr);
    pointer_level = inner_ptr > 0 ? inner_ptr - 1 : 0;
    return t;
  }
  if (expr->getType() == ASTNodeType::BinaryOp) {
    const auto *b = static_cast<const BinaryOpNode *>(expr);
    int l_ptr = 0;
    DataType lt = get_expr_type(b->left.get(), l_ptr);
    if (l_ptr > 0) {
      pointer_level = l_ptr;
      return lt;
    }
    int r_ptr = 0;
    DataType rt = get_expr_type(b->right.get(), r_ptr);
    if (r_ptr > 0) {
      pointer_level = r_ptr;
      return rt;
    }
    return lt;
  }
  if (expr->getType() == ASTNodeType::StringLit) {
    pointer_level = 1;
    return DataType::Char;
  }
  return DataType::Int;
}

int X86_64CodeGenerator::get_type_size(DataType type, const std::string &st_name) const {
  if (type == DataType::Struct || type == DataType::Union) {
    auto it = struct_defs_.find(st_name);
    if (it != struct_defs_.end()) {
      return it->second.total_size > 0 ? it->second.total_size : 8;
    }
    return 8;
  }
  if (type == DataType::Char) return 1;
  if (type == DataType::Short) return 2;
  return 8;
}

std::string X86_64CodeGenerator::get_expr_struct_type(const ASTNode *expr) const {
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
  } else if (node->getType() == ASTNodeType::StructDef) {
    const auto *sd = static_cast<const StructDefNode *>(node);
    if (sd->var_decl) {
      size += compute_ast_frame_bytes(sd->var_decl.get());
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
  is_func_ptr_.clear();
  var_struct_name_.clear();
  struct_defs_.clear();
  current_offset_ = 0;
  label_counter_ = 0;
  break_stack_.clear();
  continue_stack_.clear();
  string_literals_.clear();

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

  if (!has_func_defs) {
    std::ostringstream body_ss;
    std::ostream *orig_out = out_;
    out_ = &body_ss;
    ret_label_ = ".Lmain_ret";

    program.accept(*this);

    out_ = orig_out;
    int frame_size = (current_offset_ + 16 > 128) ? ((current_offset_ + 16 + 15) & ~15) : 128;

    emit_raw(".globl main");
    emit_raw("main:");
    emit_line("pushq %rbp");
    emit_line("movq %rsp, %rbp");
    emit_line("subq $" + std::to_string(frame_size) + ", %rsp");
    *out_ << body_ss.str();
    emit_raw(ret_label_ + ":");
    emit_line("leave");
    emit_line("ret");
    if (!string_literals_.empty()) {
      emit_raw(".section .rodata");
      for (const auto &lit : string_literals_) {
        emit_raw(lit.first + ":");
        emit_string_literal(lit.second);
      }
      emit_raw(".text");
    }
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
    var_pointer_level_.clear();
    is_array_.clear();
    array_sizes_.clear();
    is_func_ptr_.clear();
    current_offset_ = 0;

    std::ostringstream body_ss;
    std::ostream *orig_out = out_;
    out_ = &body_ss;
    ret_label_ = ".Lmain_ret";

    for (const auto *stmt : non_func_stmts) {
      stmt->accept(*this);
    }

    out_ = orig_out;
    int frame_size = (current_offset_ + 16 > 128) ? ((current_offset_ + 16 + 15) & ~15) : 128;

    emit_raw(".globl main");
    emit_raw("main:");
    emit_line("pushq %rbp");
    emit_line("movq %rsp, %rbp");
    emit_line("subq $" + std::to_string(frame_size) + ", %rsp");
    *out_ << body_ss.str();
    emit_raw(ret_label_ + ":");
    emit_line("leave");
    emit_line("ret");
  }

  if (!string_literals_.empty()) {
    emit_raw(".section .rodata");
    for (const auto &lit : string_literals_) {
      emit_raw(lit.first + ":");
      emit_string_literal(lit.second);
    }
    emit_raw(".text");
  }
}

void X86_64CodeGenerator::visit(const FuncDefNode &node) {
  var_offsets_.clear();
  var_types_.clear();
  var_pointer_level_.clear();
  is_array_.clear();
  array_sizes_.clear();
  is_func_ptr_.clear();
  current_offset_ = 0;
  break_stack_.clear();
  continue_stack_.clear();

  std::string prev_ret_label = ret_label_;
  ret_label_ = ".L" + node.name + "_ret";

  static const char *const ARG_REGS[] = {
      "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

  std::ostringstream param_ss;
  for (size_t i = 0; i < node.params.size(); ++i) {
    if (i < 6) {
      const auto &p = node.params[i];
      var_types_[p.name] = p.type;
      var_pointer_level_[p.name] = p.pointer_level;
      if (p.is_func_ptr) {
        is_func_ptr_[p.name] = true;
      }
      int offset = get_var_offset(p.name);
      param_ss << "    movq " << ARG_REGS[i] << ", -"
               << offset << "(%rbp)\n";
    } else {
      unsupported(node, "more than 6 function parameters");
    }
  }

  std::ostringstream body_ss;
  std::ostream *orig_out = out_;
  out_ = &body_ss;

  if (node.body) {
    node.body->accept(*this);
  }

  out_ = orig_out;

  int frame_size = (current_offset_ + 16 > 128) ? ((current_offset_ + 16 + 15) & ~15) : 128;

  emit_raw(".globl " + node.name);
  emit_raw(node.name + ":");
  emit_line("pushq %rbp");
  emit_line("movq %rsp, %rbp");
  emit_line("subq $" + std::to_string(frame_size) + ", %rsp");

  *out_ << param_ss.str();
  *out_ << body_ss.str();

  emit_raw(ret_label_ + ":");
  emit_line("leave");
  emit_line("ret");

  ret_label_ = prev_ret_label;
}

void X86_64CodeGenerator::visit(const CallNode &node) {
  if (node.args.size() > 6) {
    unsupported(node, "more than 6 function call arguments");
  }

  bool is_indirect = false;
  if (node.callee_expr) {
    is_indirect = true;
    node.callee_expr->accept(*this);
    emit_line("pushq %rax");
  } else if (!node.callee.empty() && var_offsets_.find(node.callee) != var_offsets_.end()) {
    is_indirect = true;
    int offset = get_var_offset(node.callee);
    emit_line("movq -" + std::to_string(offset) + "(%rbp), %rax");
    emit_line("pushq %rax");
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

  // Dual-load Windows x64 ABI registers so external functions (e.g. puts, printf)
  // work seamlessly on Windows (MinGW/MSVC) as well as SysV AMD64 (Linux).
  if (node.args.size() >= 4) emit_line("movq %rcx, %r9");
  if (node.args.size() >= 3) emit_line("movq %rdx, %r8");
  if (node.args.size() >= 2) emit_line("movq %rsi, %rdx");
  if (node.args.size() >= 1) emit_line("movq %rdi, %rcx");

  if (is_indirect) {
    emit_line("popq %r11");
    emit_line("movq %rsp, %r10");
    emit_line("andq $-16, %rsp");
    emit_line("pushq %r10");
    emit_line("subq $40, %rsp");
    emit_line("call *%r11");
    emit_line("addq $40, %rsp");
    emit_line("popq %rsp");
  } else {
    emit_line("movq %rsp, %r11");
    emit_line("andq $-16, %rsp");
    emit_line("pushq %r11");
    emit_line("subq $40, %rsp");
    emit_line("call " + node.callee);
    emit_line("addq $40, %rsp");
    emit_line("popq %rsp");
  }
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
  var_pointer_level_[node.name] = node.pointer_level;
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
      if (node.data_type == DataType::Char && node.pointer_level == 0 && !is_func_ptr(node.name)) {
        emit_line("movb %al, -" + std::to_string(offset) + "(%rbp)");
      } else if (node.data_type == DataType::Short && node.pointer_level == 0 && !is_func_ptr(node.name)) {
        emit_line("movw %ax, -" + std::to_string(offset) + "(%rbp)");
      } else {
        emit_line("movq %rax, -" + std::to_string(offset) + "(%rbp)");
      }
    }
    return;
  }
  if (node.is_array) {
    is_array_[node.name] = true;
    array_sizes_[node.name] = node.array_size;
    int elem_size = 8;
    if (node.data_type == DataType::Char && node.pointer_level == 0) elem_size = 1;
    else if (node.data_type == DataType::Short && node.pointer_level == 0) elem_size = 2;
    else if (node.data_type == DataType::Struct || node.data_type == DataType::Union) {
      elem_size = get_type_size(node.data_type, node.struct_name);
    }
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
          if (elem_size == 1) {
            emit_line("movb %al, -" + std::to_string(elem_offset) + "(%rbp)");
          } else if (elem_size == 2) {
            emit_line("movw %ax, -" + std::to_string(elem_offset) + "(%rbp)");
          } else {
            emit_line("movq %rax, -" + std::to_string(elem_offset) + "(%rbp)");
          }
        }
      }
    }
  } else {
    int st_size = get_type_size(node.data_type, node.struct_name);
    current_offset_ += st_size;
    if (current_offset_ % 8 != 0) {
      current_offset_ += 8 - (current_offset_ % 8);
    }
    int base_offset = current_offset_;
    var_offsets_[node.name] = base_offset;
    if (node.init) {
      if (node.init->getType() == ASTNodeType::InitList) {
        const auto *init_list = static_cast<const InitListNode *>(node.init.get());
        const auto &st_info = struct_defs_[node.struct_name];
        for (size_t i = 0; i < init_list->elements.size() && i < st_info.members.size(); ++i) {
          if (init_list->elements[i]) {
            init_list->elements[i]->accept(*this);
            int elem_offset = base_offset - st_info.members[i].offset;
            if (st_info.members[i].type == DataType::Char && st_info.members[i].pointer_level == 0) {
              emit_line("movb %al, -" + std::to_string(elem_offset) + "(%rbp)");
            } else if (st_info.members[i].type == DataType::Short && st_info.members[i].pointer_level == 0) {
              emit_line("movw %ax, -" + std::to_string(elem_offset) + "(%rbp)");
            } else {
              emit_line("movq %rax, -" + std::to_string(elem_offset) + "(%rbp)");
            }
          }
        }
      } else {
        node.init->accept(*this);
        emit_line("movq %rax, -" + std::to_string(base_offset) + "(%rbp)");
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

  bool is_local_arr = false;
  auto arr_it = is_array_.find(ident->name);
  if (arr_it != is_array_.end() && arr_it->second) {
    is_local_arr = true;
  }

  if (is_local_arr) {
    emit_line("leaq -" + std::to_string(base_offset) + "(%rbp), %r11");
  } else {
    emit_line("movq -" + std::to_string(base_offset) + "(%rbp), %r11");
  }

  int ptr_level = 0;
  DataType elem_type = get_expr_type(ident, ptr_level);
  bool is_elem_char = (elem_type == DataType::Char && ptr_level <= 1);
  bool is_elem_short = (elem_type == DataType::Short && ptr_level <= 1);
  if (is_elem_char) {
    emit_line("movsbq (%r11, %rax, 1), %rax");
  } else if (is_elem_short) {
    emit_line("movswq (%r11, %rax, 2), %rax");
  } else {
    int elem_size = 8;
    if (elem_type == DataType::Struct || elem_type == DataType::Union) {
      auto st_it = var_struct_name_.find(ident->name);
      if (st_it != var_struct_name_.end()) {
        elem_size = get_type_size(elem_type, st_it->second);
      }
    }
    if (elem_size == 8) {
      emit_line("movq (%r11, %rax, 8), %rax");
    } else {
      emit_line("imulq $" + std::to_string(elem_size) + ", %rax");
      emit_line("movq (%r11, %rax), %rax");
    }
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
    bool is_local_arr = false;
    auto arr_it = is_array_.find(ident->name);
    if (arr_it != is_array_.end() && arr_it->second) {
      is_local_arr = true;
    }

    if (is_local_arr) {
      emit_line("leaq -" + std::to_string(base_offset) + "(%rbp), %r11");
    } else {
      emit_line("movq -" + std::to_string(base_offset) + "(%rbp), %r11");
    }

    int ptr_level = 0;
    DataType elem_type = get_expr_type(ident, ptr_level);
    bool is_elem_char = (elem_type == DataType::Char && ptr_level <= 1);
    bool is_elem_short = (elem_type == DataType::Short && ptr_level <= 1);
    if (is_elem_char) {
      emit_line("movb %al, (%r11, %r10, 1)");
    } else if (is_elem_short) {
      emit_line("movw %ax, (%r11, %r10, 2)");
    } else {
      int elem_size = 8;
      if (elem_type == DataType::Struct || elem_type == DataType::Union) {
        auto st_it = var_struct_name_.find(ident->name);
        if (st_it != var_struct_name_.end()) {
          elem_size = get_type_size(elem_type, st_it->second);
        }
      }
      if (elem_size == 8) {
        emit_line("movq %rax, (%r11, %r10, 8)");
      } else {
        emit_line("imulq $" + std::to_string(elem_size) + ", %r10");
        emit_line("movq %rax, (%r11, %r10)");
      }
    }
    return;
  }

  if (node.left->getType() == ASTNodeType::UnaryOp &&
      static_cast<const UnaryOpNode *>(node.left.get())->op == TOK_STAR) {
    auto *unary = static_cast<const UnaryOpNode *>(node.left.get());
    if (unary->expr) {
      unary->expr->accept(*this);
    } else {
      emit_line("movq $0, %rax");
    }
    emit_line("pushq %rax");

    if (node.right) {
      node.right->accept(*this);
    }

    emit_line("popq %r10");
    int ptr_level = 0;
    DataType pt = get_expr_type(unary->expr.get(), ptr_level);
    if (pt == DataType::Char && ptr_level == 1) {
      emit_line("movb %al, (%r10)");
    } else if (pt == DataType::Short && ptr_level == 1) {
      emit_line("movw %ax, (%r10)");
    } else {
      emit_line("movq %rax, (%r10)");
    }
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
      emit_line("pushq %rax");
      mem->object->accept(*this);
      if (m.offset > 0) {
        emit_line("addq $" + std::to_string(m.offset) + ", %rax");
      }
      emit_line("movq %rax, %r11");
      emit_line("popq %rax");
      if (m.type == DataType::Char && m.pointer_level == 0) {
        emit_line("movb %al, (%r11)");
      } else if (m.type == DataType::Short && m.pointer_level == 0) {
        emit_line("movw %ax, (%r11)");
      } else {
        emit_line("movq %rax, (%r11)");
      }
      return;
    } else {
      if (mem->object->getType() == ASTNodeType::Identifier) {
        auto *ident = static_cast<const IdentifierNode *>(mem->object.get());
        int base_offset = get_var_offset(ident->name);
        int mem_offset = base_offset - m.offset;
        if (node.right) {
          node.right->accept(*this);
        }
        if (m.type == DataType::Char && m.pointer_level == 0) {
          emit_line("movb %al, -" + std::to_string(mem_offset) + "(%rbp)");
        } else if (m.type == DataType::Short && m.pointer_level == 0) {
          emit_line("movw %ax, -" + std::to_string(mem_offset) + "(%rbp)");
        } else {
          emit_line("movq %rax, -" + std::to_string(mem_offset) + "(%rbp)");
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
  if (get_var_type(ident->name) == DataType::Char && get_var_pointer_level(ident->name) == 0 && !is_func_ptr(ident->name)) {
    emit_line("movb %al, -" + std::to_string(offset) + "(%rbp)");
  } else if (get_var_type(ident->name) == DataType::Short && get_var_pointer_level(ident->name) == 0 && !is_func_ptr(ident->name)) {
    emit_line("movw %ax, -" + std::to_string(offset) + "(%rbp)");
  } else {
    emit_line("movq %rax, -" + std::to_string(offset) + "(%rbp)");
  }
}

void X86_64CodeGenerator::visit(const IntLitNode &node) {
  emit_line("movq $" + std::to_string(node.val) + ", %rax");
}

void X86_64CodeGenerator::visit(const StringLitNode &node) {
  std::string lbl = new_label(".LC");
  string_literals_.push_back({lbl, node.value});
  emit_line("leaq " + lbl + "(%rip), %rax");
}

void X86_64CodeGenerator::visit(const IdentifierNode &node) {
  auto it_offset = var_offsets_.find(node.name);
  if (it_offset == var_offsets_.end()) {
    // Function address decay / symbol reference
    emit_line("leaq " + node.name + "(%rip), %rax");
    return;
  }
  int offset = it_offset->second;
  auto it = is_array_.find(node.name);
  if (it != is_array_.end() && it->second) {
    emit_line("leaq -" + std::to_string(offset) + "(%rbp), %rax");
    return;
  }
  if (get_var_type(node.name) == DataType::Char && get_var_pointer_level(node.name) == 0 && !is_func_ptr(node.name)) {
    emit_line("movsbq -" + std::to_string(offset) + "(%rbp), %rax");
  } else if (get_var_type(node.name) == DataType::Short && get_var_pointer_level(node.name) == 0 && !is_func_ptr(node.name)) {
    emit_line("movswq -" + std::to_string(offset) + "(%rbp), %rax");
  } else {
    emit_line("movq -" + std::to_string(offset) + "(%rbp), %rax");
  }
}

void X86_64CodeGenerator::visit(const UnaryOpNode &node) {
  if (node.op == TOK_AMP) {
    if (!node.expr) {
      unsupported(node, "empty address-of");
    }
    if (node.expr->getType() == ASTNodeType::Identifier) {
      auto *ident = static_cast<const IdentifierNode *>(node.expr.get());
      auto it = var_offsets_.find(ident->name);
      if (it != var_offsets_.end()) {
        emit_line("leaq -" + std::to_string(it->second) + "(%rbp), %rax");
      } else {
        emit_line("leaq " + ident->name + "(%rip), %rax");
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
        emit_line("movq $0, %rax");
      }
      bool is_local_arr = false;
      auto arr_it = is_array_.find(ident->name);
      if (arr_it != is_array_.end() && arr_it->second) {
        is_local_arr = true;
      }

      if (is_local_arr) {
        emit_line("leaq -" + std::to_string(base_offset) + "(%rbp), %r11");
      } else {
        emit_line("movq -" + std::to_string(base_offset) + "(%rbp), %r11");
      }

      int ptr_level = 0;
      DataType elem_type = get_expr_type(ident, ptr_level);
      bool is_elem_char = (elem_type == DataType::Char && ptr_level <= 1);
      bool is_elem_short = (elem_type == DataType::Short && ptr_level <= 1);
      if (is_elem_char) {
        emit_line("leaq (%r11, %rax, 1), %rax");
      } else if (is_elem_short) {
        emit_line("leaq (%r11, %rax, 2), %rax");
      } else {
        int elem_size = 8;
        if (elem_type == DataType::Struct || elem_type == DataType::Union) {
          auto st_it = var_struct_name_.find(ident->name);
          if (st_it != var_struct_name_.end()) {
            elem_size = get_type_size(elem_type, st_it->second);
          }
        }
        if (elem_size == 8) {
          emit_line("leaq (%r11, %rax, 8), %rax");
        } else {
          emit_line("imulq $" + std::to_string(elem_size) + ", %rax");
          emit_line("leaq (%r11, %rax), %rax");
        }
      }
      return;
    }
    if (node.expr->getType() == ASTNodeType::MemberAccess) {
      auto *mem = static_cast<const MemberAccessNode *>(node.expr.get());
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
        mem->object->accept(*this);
        if (m.offset > 0) {
          emit_line("addq $" + std::to_string(m.offset) + ", %rax");
        }
        return;
      } else {
        if (mem->object->getType() == ASTNodeType::Identifier) {
          auto *ident = static_cast<const IdentifierNode *>(mem->object.get());
          int base_offset = get_var_offset(ident->name);
          int mem_offset = base_offset - m.offset;
          emit_line("leaq -" + std::to_string(mem_offset) + "(%rbp), %rax");
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
      emit_line("movq $0, %rax");
    }
    int ptr_level = 0;
    DataType pt = get_expr_type(node.expr.get(), ptr_level);
    if (pt == DataType::Char && ptr_level == 1) {
      emit_line("movsbq (%rax), %rax");
    } else if (pt == DataType::Short && ptr_level == 1) {
      emit_line("movswq (%rax), %rax");
    } else {
      emit_line("movq (%rax), %rax");
    }
    return;
  }

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

void X86_64CodeGenerator::visit(const StructDefNode &node) {
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

void X86_64CodeGenerator::visit(const MemberAccessNode &node) {
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
      emit_line("addq $" + std::to_string(m.offset) + ", %rax");
    }
    if (m.type == DataType::Char && m.pointer_level == 0) {
      emit_line("movsbq (%rax), %rax");
    } else if (m.type == DataType::Short && m.pointer_level == 0) {
      emit_line("movswq (%rax), %rax");
    } else {
      emit_line("movq (%rax), %rax");
    }
  } else {
    if (node.object->getType() == ASTNodeType::Identifier) {
      auto *ident = static_cast<const IdentifierNode *>(node.object.get());
      int base_offset = get_var_offset(ident->name);
      int mem_offset = base_offset - m.offset;
      if (m.type == DataType::Char && m.pointer_level == 0) {
        emit_line("movsbq -" + std::to_string(mem_offset) + "(%rbp), %rax");
      } else if (m.type == DataType::Short && m.pointer_level == 0) {
        emit_line("movswq -" + std::to_string(mem_offset) + "(%rbp), %rax");
      } else {
        emit_line("movq -" + std::to_string(mem_offset) + "(%rbp), %rax");
      }
    } else {
      unsupported(node, "complex member access");
    }
  }
}
