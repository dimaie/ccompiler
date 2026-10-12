#include "codegen_c.hpp"

void CCodeGenerator::print_indent() {
  for (int i = 0; i < indent_; ++i) {
    *out_ << "  ";
  }
}

bool CCodeGenerator::statement_needs_semicolon(const ASTNode *node) {
  if (!node) return false;
  switch (node->getType()) {
  case ASTNodeType::Compound:
  case ASTNodeType::If:
  case ASTNodeType::For:
  case ASTNodeType::While:
  case ASTNodeType::DoWhile:
  case ASTNodeType::Switch:
  case ASTNodeType::Case:
  case ASTNodeType::Label:
  case ASTNodeType::FuncDef:
  case ASTNodeType::StructDef:
    return false;
  default:
    return true;
  }
}

void CCodeGenerator::emit_statement(const ASTNode *stmt) {
  if (!stmt) {
    *out_ << ";\n";
    return;
  }
  stmt->accept(*this);
  if (statement_needs_semicolon(stmt)) {
    *out_ << ";\n";
  } else {
    *out_ << "\n";
  }
}

void CCodeGenerator::generate(const ProgramNode &program, std::ostream &out) {
  out_ = &out;
  indent_ = 0;

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
    *out_ << "int main(void) {\n";
    indent_ = 1;
    program.accept(*this);
    *out_ << "}\n";
    return;
  }

  if (has_main_func) {
    for (const auto &stmt : program.statements) {
      if (stmt) {
        print_indent();
        emit_statement(stmt.get());
      }
    }
    return;
  }

  // Program has functions, but no main() defined:
  // Emit all StructDefs and FuncDefs at top level
  for (const auto &stmt : program.statements) {
    if (stmt && (stmt->getType() == ASTNodeType::FuncDef ||
                 stmt->getType() == ASTNodeType::StructDef)) {
      stmt->accept(*this);
    }
  }

  // Synthesize main() for non-function, non-struct statements
  if (!non_func_stmts.empty()) {
    *out_ << "int main(void) {\n";
    indent_ = 1;
    for (const auto *stmt : non_func_stmts) {
      if (stmt->getType() == ASTNodeType::StructDef) {
        // StructDefs were already emitted at top level
        continue;
      }
      print_indent();
      emit_statement(stmt);
    }
    indent_ = 0;
    *out_ << "}\n";
  }
}

void CCodeGenerator::visit(const ProgramNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      print_indent();
      emit_statement(stmt.get());
    }
  }
}

void CCodeGenerator::visit(const FuncDefNode &node) {
  if (node.is_static) {
    *out_ << "static ";
  }
  if (node.calling_convention != CallingConvention::Default) {
    *out_ << calling_convention_str(node.calling_convention) << " ";
  }
  if (node.return_type == DataType::Struct || node.return_type == DataType::Union) {
    *out_ << (node.return_type == DataType::Union ? "union " : "struct ")
          << node.return_struct_name;
  } else {
    *out_ << data_type_str(node.return_type);
  }
  if (node.return_pointer_level > 0) {
    *out_ << " ";
    for (int i = 0; i < node.return_pointer_level; ++i) *out_ << "*";
    *out_ << node.name << "(";
  } else {
    *out_ << " " << node.name << "(";
  }
  if (node.params.empty()) {
    if (node.is_variadic) {
      *out_ << "...";
    } else {
      *out_ << "void";
    }
  } else {
    for (size_t i = 0; i < node.params.size(); ++i) {
      if (i > 0) {
        *out_ << ", ";
      }
      const auto &p = node.params[i];
      if (p.is_func_ptr) {
        *out_ << data_type_str(p.func_ptr_ret_type);
        if (p.func_ptr_ret_pointer_level > 0) {
          *out_ << " ";
          for (int j = 0; j < p.func_ptr_ret_pointer_level; ++j) *out_ << "*";
          *out_ << "(*" << p.name << ")(";
        } else {
          *out_ << " (*" << p.name << ")(";
        }
        for (size_t j = 0; j < p.func_ptr_params.size(); ++j) {
          if (j > 0) *out_ << ", ";
          *out_ << data_type_str(p.func_ptr_params[j]);
        }
        if (p.func_ptr_is_variadic) {
          if (!p.func_ptr_params.empty()) *out_ << ", ";
          *out_ << "...";
        }
        *out_ << ")";
      } else {
        if (p.type == DataType::Struct || p.type == DataType::Union) {
          *out_ << (p.type == DataType::Union ? "union " : "struct ") << p.struct_name;
        } else {
          *out_ << data_type_str(p.type);
        }
        if (p.pointer_level > 0) {
          *out_ << " ";
          for (int j = 0; j < p.pointer_level; ++j) *out_ << "*";
          *out_ << p.name;
        } else {
          *out_ << " " << p.name;
        }
      }
    }
    if (node.is_variadic) {
      *out_ << ", ...";
    }
  }
  if (node.body) {
    *out_ << ") ";
    node.body->accept(*this);
  } else {
    *out_ << ");";
  }
  *out_ << "\n";
}

void CCodeGenerator::visit(const CallNode &node) {
  if (node.callee_expr) {
    node.callee_expr->accept(*this);
    *out_ << "(";
  } else {
    *out_ << node.callee << "(";
  }
  for (size_t i = 0; i < node.args.size(); ++i) {
    if (i > 0) {
      *out_ << ", ";
    }
    if (node.args[i]) {
      node.args[i]->accept(*this);
    }
  }
  *out_ << ")";
}

void CCodeGenerator::visit(const CompoundNode &node) {
  *out_ << "{\n";
  indent_++;
  for (const auto &stmt : node.statements) {
    if (stmt) {
      print_indent();
      emit_statement(stmt.get());
    }
  }
  indent_--;
  print_indent();
  *out_ << "}";
}

void CCodeGenerator::visit(const VarDeclNode &node) {
  if (node.is_func_ptr) {
    *out_ << data_type_str(node.func_ptr_ret_type);
    if (node.func_ptr_ret_pointer_level > 0) {
      *out_ << " ";
      for (int i = 0; i < node.func_ptr_ret_pointer_level; ++i) *out_ << "*";
      *out_ << "(*" << node.name << ")(";
    } else {
      *out_ << " (*" << node.name << ")(";
    }
    for (size_t i = 0; i < node.func_ptr_params.size(); ++i) {
      if (i > 0) *out_ << ", ";
      *out_ << data_type_str(node.func_ptr_params[i]);
    }
    *out_ << ")";
  } else {
    if (node.data_type == DataType::Struct || node.data_type == DataType::Union) {
      *out_ << (node.data_type == DataType::Union ? "union " : "struct ") << node.struct_name;
    } else {
      *out_ << data_type_str(node.data_type);
    }
    if (node.pointer_level > 0) {
      *out_ << " ";
      for (int i = 0; i < node.pointer_level; ++i) *out_ << "*";
      *out_ << node.name;
    } else {
      *out_ << " " << node.name;
    }
    if (node.is_array) {
      *out_ << "[" << node.array_size << "]";
    }
  }
  if (node.init) {
    *out_ << " = ";
    node.init->accept(*this);
  }
}

void CCodeGenerator::visit(const ArrayAccessNode &node) {
  if (node.array) {
    node.array->accept(*this);
  }
  *out_ << "[";
  if (node.index) {
    node.index->accept(*this);
  }
  *out_ << "]";
}

void CCodeGenerator::visit(const InitListNode &node) {
  *out_ << "{";
  for (size_t i = 0; i < node.elements.size(); ++i) {
    if (i > 0) {
      *out_ << ", ";
    }
    if (node.elements[i]) {
      node.elements[i]->accept(*this);
    }
  }
  *out_ << "}";
}

void CCodeGenerator::visit(const AssignNode &node) {
  if (node.left) node.left->accept(*this);
  *out_ << " = ";
  if (node.right) node.right->accept(*this);
}

void CCodeGenerator::visit(const IntLitNode &node) {
  *out_ << node.val;
}

void CCodeGenerator::visit(const StringLitNode &node) {
  *out_ << "\"";
  for (unsigned char c : node.value) {
    if (c == '"') *out_ << "\\\"";
    else if (c == '\\') *out_ << "\\\\";
    else if (c == '\n') *out_ << "\\n";
    else if (c == '\t') *out_ << "\\t";
    else if (c == '\r') *out_ << "\\r";
    else if (c >= 32 && c <= 126) *out_ << static_cast<char>(c);
    else {
      *out_ << "\\" << std::oct << static_cast<int>(c) << std::dec;
    }
  }
  *out_ << "\"";
}

void CCodeGenerator::visit(const IdentifierNode &node) {
  *out_ << node.name;
}

void CCodeGenerator::visit(const UnaryOpNode &node) {
  if (node.op == TOK_STAR) {
    *out_ << "(*";
    if (node.expr) node.expr->accept(*this);
    *out_ << ")";
    return;
  }
  *out_ << token_op_str(node.op);
  if (node.op == TOK_MINUS || node.op == TOK_PLUS) {
    *out_ << " ";
  }
  if (node.expr) node.expr->accept(*this);
}

void CCodeGenerator::visit(const BinaryOpNode &node) {
  *out_ << "(";
  if (node.left) node.left->accept(*this);
  *out_ << " " << token_op_str(node.op) << " ";
  if (node.right) node.right->accept(*this);
  *out_ << ")";
}

void CCodeGenerator::visit(const IfNode &node) {
  *out_ << "if (";
  if (node.cond) node.cond->accept(*this);
  *out_ << ") ";
  if (!node.then_branch) {
    *out_ << ";";
  } else if (node.then_branch->getType() == ASTNodeType::Compound) {
    node.then_branch->accept(*this);
  } else {
    node.then_branch->accept(*this);
    if (statement_needs_semicolon(node.then_branch.get())) {
      *out_ << ";";
    }
  }
  if (node.else_branch) {
    *out_ << " else ";
    if (node.else_branch->getType() == ASTNodeType::Compound) {
      node.else_branch->accept(*this);
    } else {
      node.else_branch->accept(*this);
      if (statement_needs_semicolon(node.else_branch.get())) {
        *out_ << ";";
      }
    }
  }
}

void CCodeGenerator::visit(const ForNode &node) {
  *out_ << "for (";
  if (node.init) {
    node.init->accept(*this);
  }
  *out_ << "; ";
  if (node.cond) {
    node.cond->accept(*this);
  }
  *out_ << "; ";
  if (node.step) {
    node.step->accept(*this);
  }
  *out_ << ") ";
  if (!node.body) {
    *out_ << ";";
  } else if (node.body->getType() == ASTNodeType::Compound) {
    node.body->accept(*this);
  } else {
    node.body->accept(*this);
    if (statement_needs_semicolon(node.body.get())) {
      *out_ << ";";
    }
  }
}

void CCodeGenerator::visit(const WhileNode &node) {
  *out_ << "while (";
  if (node.cond) {
    node.cond->accept(*this);
  }
  *out_ << ") ";
  if (!node.body) {
    *out_ << ";";
  } else if (node.body->getType() == ASTNodeType::Compound) {
    node.body->accept(*this);
  } else {
    node.body->accept(*this);
    if (statement_needs_semicolon(node.body.get())) {
      *out_ << ";";
    }
  }
}

void CCodeGenerator::visit(const DoWhileNode &node) {
  *out_ << "do ";
  if (!node.body) {
    *out_ << ";";
  } else if (node.body->getType() == ASTNodeType::Compound) {
    node.body->accept(*this);
  } else {
    node.body->accept(*this);
    if (statement_needs_semicolon(node.body.get())) {
      *out_ << ";";
    }
  }
  *out_ << " while (";
  if (node.cond) {
    node.cond->accept(*this);
  }
  *out_ << ");";
}

void CCodeGenerator::visit(const SwitchNode &node) {
  *out_ << "switch (";
  if (node.cond) {
    node.cond->accept(*this);
  }
  *out_ << ") {\n";
  indent_++;
  for (const auto &c : node.cases) {
    if (c) {
      c->accept(*this);
    }
  }
  indent_--;
  print_indent();
  *out_ << "}";
}

void CCodeGenerator::visit(const CaseNode &node) {
  print_indent();
  if (node.is_default) {
    *out_ << "default:\n";
  } else {
    *out_ << "case ";
    if (node.value) {
      node.value->accept(*this);
    }
    *out_ << ":\n";
  }
  indent_++;
  for (const auto &stmt : node.statements) {
    if (stmt) {
      print_indent();
      emit_statement(stmt.get());
    }
  }
  indent_--;
}

void CCodeGenerator::visit(const BreakNode &) {
  *out_ << "break";
}

void CCodeGenerator::visit(const ContinueNode &) {
  *out_ << "continue";
}

void CCodeGenerator::visit(const GotoNode &node) {
  *out_ << "goto " << node.label;
}

void CCodeGenerator::visit(const LabelNode &node) {
  *out_ << node.name << ":";
}

void CCodeGenerator::visit(const ReturnNode &node) {
  *out_ << "return ";
  if (node.expr) node.expr->accept(*this);
}

void CCodeGenerator::visit(const StructDefNode &node) {
  *out_ << (node.is_union ? "union " : "struct ");
  if (!node.name.empty()) {
    *out_ << node.name << " ";
  }
  *out_ << "{\n";
  indent_++;
  for (const auto &m : node.members) {
    print_indent();
    if (m.type == DataType::Struct || m.type == DataType::Union) {
      *out_ << (m.type == DataType::Union ? "union " : "struct ") << m.struct_name;
    } else {
      *out_ << data_type_str(m.type);
    }
    if (m.pointer_level > 0) {
      *out_ << " ";
      for (int i = 0; i < m.pointer_level; ++i) *out_ << "*";
      *out_ << m.name;
    } else {
      *out_ << " " << m.name;
    }
    if (m.is_array) {
      *out_ << "[" << m.array_size << "]";
    }
    *out_ << ";\n";
  }
  indent_--;
  print_indent();
  *out_ << "}";
  if (node.var_decl) {
    *out_ << " ";
    if (node.var_decl->pointer_level > 0) {
      for (int i = 0; i < node.var_decl->pointer_level; ++i) *out_ << "*";
    }
    *out_ << node.var_decl->name;
    if (node.var_decl->is_array) {
      *out_ << "[" << node.var_decl->array_size << "]";
    }
    if (node.var_decl->init) {
      *out_ << " = ";
      node.var_decl->init->accept(*this);
    }
  }
  *out_ << ";\n";
}

void CCodeGenerator::visit(const MemberAccessNode &node) {
  if (node.object) {
    node.object->accept(*this);
  }
  *out_ << (node.is_arrow ? "->" : ".");
  *out_ << node.member;
}

void CCodeGenerator::visit(const AsmStmtNode &node) {
  *out_ << "__asm__(\"";
  for (char c : node.asm_code) {
    if (c == '"') *out_ << "\\\"";
    else if (c == '\\') *out_ << "\\\\";
    else if (c == '\n') *out_ << "\\n";
    else if (c == '\t') *out_ << "\\t";
    else *out_ << c;
  }
  *out_ << "\");\n";
}


