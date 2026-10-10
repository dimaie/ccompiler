#include "codegen_c.hpp"

void CCodeGenerator::print_indent() {
  for (int i = 0; i < indent_; ++i) {
    *out_ << "  ";
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

  for (const auto &stmt : program.statements) {
    if (stmt && stmt->getType() == ASTNodeType::FuncDef) {
      stmt->accept(*this);
    }
  }

  if (!has_main_func && !non_func_stmts.empty()) {
    *out_ << "int main(void) {\n";
    indent_ = 1;
    for (const auto *stmt : non_func_stmts) {
      print_indent();
      stmt->accept(*this);
      if (stmt->getType() != ASTNodeType::Compound &&
          stmt->getType() != ASTNodeType::If &&
          stmt->getType() != ASTNodeType::For &&
          stmt->getType() != ASTNodeType::While &&
          stmt->getType() != ASTNodeType::DoWhile &&
          stmt->getType() != ASTNodeType::Switch &&
          stmt->getType() != ASTNodeType::Label &&
          stmt->getType() != ASTNodeType::FuncDef) {
        *out_ << ";\n";
      } else {
        *out_ << "\n";
      }
    }
    indent_ = 0;
    *out_ << "}\n";
  }
}

void CCodeGenerator::visit(const ProgramNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      print_indent();
      stmt->accept(*this);
      if (stmt->getType() != ASTNodeType::Compound &&
          stmt->getType() != ASTNodeType::If &&
          stmt->getType() != ASTNodeType::For &&
          stmt->getType() != ASTNodeType::While &&
          stmt->getType() != ASTNodeType::DoWhile &&
          stmt->getType() != ASTNodeType::Switch &&
          stmt->getType() != ASTNodeType::Label &&
          stmt->getType() != ASTNodeType::FuncDef) {
        *out_ << ";\n";
      } else {
        *out_ << "\n";
      }
    }
  }
}

void CCodeGenerator::visit(const FuncDefNode &node) {
  *out_ << data_type_str(node.return_type) << " " << node.name << "(";
  if (node.params.empty()) {
    *out_ << "void";
  } else {
    for (size_t i = 0; i < node.params.size(); ++i) {
      if (i > 0) {
        *out_ << ", ";
      }
      *out_ << data_type_str(node.params[i].type) << " " << node.params[i].name;
    }
  }
  *out_ << ") ";
  if (node.body) {
    node.body->accept(*this);
  }
  *out_ << "\n";
}

void CCodeGenerator::visit(const CallNode &node) {
  *out_ << node.callee << "(";
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
      stmt->accept(*this);
      if (stmt->getType() != ASTNodeType::Compound &&
          stmt->getType() != ASTNodeType::If &&
          stmt->getType() != ASTNodeType::For &&
          stmt->getType() != ASTNodeType::While &&
          stmt->getType() != ASTNodeType::DoWhile &&
          stmt->getType() != ASTNodeType::Switch &&
          stmt->getType() != ASTNodeType::Label &&
          stmt->getType() != ASTNodeType::FuncDef) {
        *out_ << ";\n";
      } else {
        *out_ << "\n";
      }
    }
  }
  indent_--;
  print_indent();
  *out_ << "}";
}

void CCodeGenerator::visit(const VarDeclNode &node) {
  *out_ << data_type_str(node.data_type) << " " << node.name;
  if (node.init) {
    *out_ << " = ";
    node.init->accept(*this);
  }
}

void CCodeGenerator::visit(const AssignNode &node) {
  if (node.left) node.left->accept(*this);
  *out_ << " = ";
  if (node.right) node.right->accept(*this);
}

void CCodeGenerator::visit(const IntLitNode &node) {
  *out_ << node.val;
}

void CCodeGenerator::visit(const IdentifierNode &node) {
  *out_ << node.name;
}

void CCodeGenerator::visit(const UnaryOpNode &node) {
  *out_ << token_op_str(node.op);
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
  if (node.then_branch) node.then_branch->accept(*this);
  if (node.else_branch) {
    *out_ << " else ";
    node.else_branch->accept(*this);
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
  if (node.body) {
    node.body->accept(*this);
  }
}

void CCodeGenerator::visit(const WhileNode &node) {
  *out_ << "while (";
  if (node.cond) {
    node.cond->accept(*this);
  }
  *out_ << ") ";
  if (node.body) {
    node.body->accept(*this);
  }
}

void CCodeGenerator::visit(const DoWhileNode &node) {
  *out_ << "do ";
  if (node.body) {
    node.body->accept(*this);
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
      stmt->accept(*this);
      if (stmt->getType() != ASTNodeType::Compound &&
          stmt->getType() != ASTNodeType::If &&
          stmt->getType() != ASTNodeType::For &&
          stmt->getType() != ASTNodeType::While &&
          stmt->getType() != ASTNodeType::DoWhile &&
          stmt->getType() != ASTNodeType::Switch &&
          stmt->getType() != ASTNodeType::Label) {
        *out_ << ";\n";
      } else {
        *out_ << "\n";
      }
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


