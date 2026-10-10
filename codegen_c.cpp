#include "codegen_c.hpp"

void CCodeGenerator::print_indent() {
  for (int i = 0; i < indent_; ++i) {
    *out_ << "  ";
  }
}

void CCodeGenerator::generate(const ProgramNode &program, std::ostream &out) {
  out_ = &out;
  indent_ = 0;

  *out_ << "int main(void) {\n";
  indent_ = 1;
  program.accept(*this);
  *out_ << "}\n";
}

void CCodeGenerator::visit(const ProgramNode &node) {
  for (const auto &stmt : node.statements) {
    if (stmt) {
      print_indent();
      stmt->accept(*this);
      if (stmt->getType() != ASTNodeType::Compound &&
          stmt->getType() != ASTNodeType::If &&
          stmt->getType() != ASTNodeType::For) {
        *out_ << ";\n";
      } else {
        *out_ << "\n";
      }
    }
  }
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
          stmt->getType() != ASTNodeType::For) {
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
  *out_ << "int " << node.name;
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

void CCodeGenerator::visit(const ReturnNode &node) {
  *out_ << "return ";
  if (node.expr) node.expr->accept(*this);
}
