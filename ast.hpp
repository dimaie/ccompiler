#ifndef AST_HPP
#define AST_HPP

#include "lexer.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class ASTNodeType {
  Program,
  Compound,
  VarDecl,
  If,
  For,
  While,
  DoWhile,
  Switch,
  Case,
  Break,
  Continue,
  Goto,
  Label,
  Return,
  Assign,
  BinaryOp,
  UnaryOp,
  IntLit,
  Identifier
};

inline const char *token_op_str(TokenType op) {
  switch (op) {
  case TOK_PLUS:
    return "+";
  case TOK_MINUS:
    return "-";
  case TOK_STAR:
    return "*";
  case TOK_SLASH:
    return "/";
  case TOK_PERCENT:
    return "%";
  case TOK_EQUAL:
    return "==";
  case TOK_NOT_EQUAL:
    return "!=";
  case TOK_LESS:
    return "<";
  case TOK_LESS_EQ:
    return "<=";
  case TOK_GREATER:
    return ">";
  case TOK_GREATER_EQ:
    return ">=";
  case TOK_LOGIC_AND:
    return "&&";
  case TOK_LOGIC_OR:
    return "||";
  case TOK_LOGIC_NOT:
    return "!";
  case TOK_TILDE:
    return "~";
  case TOK_ASSIGN:
    return "=";
  default:
    return "?";
  }
}

// Forward declarations of concrete AST nodes
class ProgramNode;
class CompoundNode;
class VarDeclNode;
class IfNode;
class ForNode;
class WhileNode;
class DoWhileNode;
class SwitchNode;
class CaseNode;
class BreakNode;
class ContinueNode;
class GotoNode;
class LabelNode;
class ReturnNode;
class AssignNode;
class BinaryOpNode;
class UnaryOpNode;
class IntLitNode;
class IdentifierNode;

/**
 * @brief Abstract Visitor interface for AST operations (codegen, analysis, etc.)
 */
class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  virtual void visit(const ProgramNode &node) = 0;
  virtual void visit(const CompoundNode &node) = 0;
  virtual void visit(const VarDeclNode &node) = 0;
  virtual void visit(const IfNode &node) = 0;
  virtual void visit(const ForNode &node) = 0;
  virtual void visit(const WhileNode &node) = 0;
  virtual void visit(const DoWhileNode &node) = 0;
  virtual void visit(const SwitchNode &node) = 0;
  virtual void visit(const CaseNode &node) = 0;
  virtual void visit(const BreakNode &node) = 0;
  virtual void visit(const ContinueNode &node) = 0;
  virtual void visit(const GotoNode &node) = 0;
  virtual void visit(const LabelNode &node) = 0;
  virtual void visit(const ReturnNode &node) = 0;
  virtual void visit(const AssignNode &node) = 0;
  virtual void visit(const BinaryOpNode &node) = 0;
  virtual void visit(const UnaryOpNode &node) = 0;
  virtual void visit(const IntLitNode &node) = 0;
  virtual void visit(const IdentifierNode &node) = 0;
};

class ASTNode {
public:
  int line{0};

  explicit ASTNode(int line = 0) : line(line) {}
  virtual ~ASTNode() = default;

  virtual ASTNodeType getType() const = 0;
  virtual void print(int indent = 0) const = 0;
  virtual void accept(ASTVisitor &visitor) const = 0;

protected:
  static void print_indent(int indent) {
    for (int i = 0; i < indent; ++i) {
      std::cout << "  ";
    }
  }
};

class ProgramNode : public ASTNode {
public:
  std::vector<std::unique_ptr<ASTNode>> statements;

  explicit ProgramNode(int line = 1) : ASTNode(line) {}

  ASTNodeType getType() const override { return ASTNodeType::Program; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void append(std::unique_ptr<ASTNode> stmt) {
    if (stmt) {
      statements.push_back(std::move(stmt));
    }
  }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Program\n";
    for (const auto &stmt : statements) {
      if (stmt) {
        stmt->print(indent + 1);
      }
    }
  }
};

class CompoundNode : public ASTNode {
public:
  std::vector<std::unique_ptr<ASTNode>> statements;

  explicit CompoundNode(int line = 0) : ASTNode(line) {}

  ASTNodeType getType() const override { return ASTNodeType::Compound; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void append(std::unique_ptr<ASTNode> stmt) {
    if (stmt) {
      statements.push_back(std::move(stmt));
    }
  }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Block\n";
    for (const auto &stmt : statements) {
      if (stmt) {
        stmt->print(indent + 1);
      }
    }
  }
};

class VarDeclNode : public ASTNode {
public:
  std::string name;
  std::unique_ptr<ASTNode> init;

  VarDeclNode(int line, std::string name,
              std::unique_ptr<ASTNode> init = nullptr)
      : ASTNode(line), name(std::move(name)), init(std::move(init)) {}

  ASTNodeType getType() const override { return ASTNodeType::VarDecl; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "VarDecl (" << name << ")\n";
    if (init) {
      init->print(indent + 1);
    }
  }
};

class IfNode : public ASTNode {
public:
  std::unique_ptr<ASTNode> cond;
  std::unique_ptr<ASTNode> then_branch;
  std::unique_ptr<ASTNode> else_branch;

  IfNode(int line, std::unique_ptr<ASTNode> cond,
         std::unique_ptr<ASTNode> then_branch,
         std::unique_ptr<ASTNode> else_branch = nullptr)
      : ASTNode(line), cond(std::move(cond)),
        then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}

  ASTNodeType getType() const override { return ASTNodeType::If; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "If\n";

    print_indent(indent + 1);
    std::cout << "[Condition]\n";
    if (cond) {
      cond->print(indent + 2);
    }

    print_indent(indent + 1);
    std::cout << "[Then]\n";
    if (then_branch) {
      then_branch->print(indent + 2);
    }

    if (else_branch) {
      print_indent(indent + 1);
      std::cout << "[Else]\n";
      else_branch->print(indent + 2);
    }
  }
};

class ForNode : public ASTNode {
public:
  std::unique_ptr<ASTNode> init;
  std::unique_ptr<ASTNode> cond;
  std::unique_ptr<ASTNode> step;
  std::unique_ptr<ASTNode> body;

  ForNode(int line, std::unique_ptr<ASTNode> init,
          std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> step,
          std::unique_ptr<ASTNode> body)
      : ASTNode(line), init(std::move(init)), cond(std::move(cond)),
        step(std::move(step)), body(std::move(body)) {}

  ASTNodeType getType() const override { return ASTNodeType::For; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "For\n";

    print_indent(indent + 1);
    std::cout << "[Init]\n";
    if (init) {
      init->print(indent + 2);
    }

    print_indent(indent + 1);
    std::cout << "[Condition]\n";
    if (cond) {
      cond->print(indent + 2);
    }

    print_indent(indent + 1);
    std::cout << "[Step]\n";
    if (step) {
      step->print(indent + 2);
    }

    print_indent(indent + 1);
    std::cout << "[Body]\n";
    if (body) {
      body->print(indent + 2);
    }
  }
};

class WhileNode : public ASTNode {
public:
  std::unique_ptr<ASTNode> cond;
  std::unique_ptr<ASTNode> body;

  WhileNode(int line, std::unique_ptr<ASTNode> cond,
            std::unique_ptr<ASTNode> body)
      : ASTNode(line), cond(std::move(cond)), body(std::move(body)) {}

  ASTNodeType getType() const override { return ASTNodeType::While; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "While\n";

    print_indent(indent + 1);
    std::cout << "[Condition]\n";
    if (cond) {
      cond->print(indent + 2);
    }

    print_indent(indent + 1);
    std::cout << "[Body]\n";
    if (body) {
      body->print(indent + 2);
    }
  }
};

class DoWhileNode : public ASTNode {
public:
  std::unique_ptr<ASTNode> body;
  std::unique_ptr<ASTNode> cond;

  DoWhileNode(int line, std::unique_ptr<ASTNode> body,
              std::unique_ptr<ASTNode> cond)
      : ASTNode(line), body(std::move(body)), cond(std::move(cond)) {}

  ASTNodeType getType() const override { return ASTNodeType::DoWhile; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "DoWhile\n";

    print_indent(indent + 1);
    std::cout << "[Body]\n";
    if (body) {
      body->print(indent + 2);
    }

    print_indent(indent + 1);
    std::cout << "[Condition]\n";
    if (cond) {
      cond->print(indent + 2);
    }
  }
};

class CaseNode : public ASTNode {
public:
  std::unique_ptr<ASTNode> value; // nullptr if default
  bool is_default{false};
  std::vector<std::unique_ptr<ASTNode>> statements;

  CaseNode(int line, std::unique_ptr<ASTNode> value,
           std::vector<std::unique_ptr<ASTNode>> statements,
           bool is_default = false)
      : ASTNode(line), value(std::move(value)), is_default(is_default),
        statements(std::move(statements)) {}

  ASTNodeType getType() const override { return ASTNodeType::Case; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    if (is_default) {
      std::cout << "Default\n";
    } else {
      std::cout << "Case\n";
      if (value) {
        value->print(indent + 1);
      }
    }
    for (const auto &stmt : statements) {
      if (stmt) {
        stmt->print(indent + 1);
      }
    }
  }
};

class SwitchNode : public ASTNode {
public:
  std::unique_ptr<ASTNode> cond;
  std::vector<std::unique_ptr<CaseNode>> cases;

  SwitchNode(int line, std::unique_ptr<ASTNode> cond,
             std::vector<std::unique_ptr<CaseNode>> cases)
      : ASTNode(line), cond(std::move(cond)), cases(std::move(cases)) {}

  ASTNodeType getType() const override { return ASTNodeType::Switch; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Switch\n";

    print_indent(indent + 1);
    std::cout << "[Condition]\n";
    if (cond) {
      cond->print(indent + 2);
    }

    print_indent(indent + 1);
    std::cout << "[Cases]\n";
    for (const auto &c : cases) {
      if (c) {
        c->print(indent + 2);
      }
    }
  }
};

class BreakNode : public ASTNode {
public:
  explicit BreakNode(int line = 0) : ASTNode(line) {}

  ASTNodeType getType() const override { return ASTNodeType::Break; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Break\n";
  }
};

class ContinueNode : public ASTNode {
public:
  explicit ContinueNode(int line = 0) : ASTNode(line) {}

  ASTNodeType getType() const override { return ASTNodeType::Continue; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Continue\n";
  }
};

class GotoNode : public ASTNode {
public:
  std::string label;

  GotoNode(int line, std::string label)
      : ASTNode(line), label(std::move(label)) {}

  ASTNodeType getType() const override { return ASTNodeType::Goto; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Goto (" << label << ")\n";
  }
};

class LabelNode : public ASTNode {
public:
  std::string name;

  LabelNode(int line, std::string name)
      : ASTNode(line), name(std::move(name)) {}

  ASTNodeType getType() const override { return ASTNodeType::Label; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Label (" << name << ")\n";
  }
};

class ReturnNode : public ASTNode {
public:
  std::unique_ptr<ASTNode> expr;

  ReturnNode(int line, std::unique_ptr<ASTNode> expr)
      : ASTNode(line), expr(std::move(expr)) {}

  ASTNodeType getType() const override { return ASTNodeType::Return; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Return\n";
    if (expr) {
      expr->print(indent + 1);
    }
  }
};

class AssignNode : public ASTNode {
public:
  std::unique_ptr<ASTNode> left;
  std::unique_ptr<ASTNode> right;

  AssignNode(int line, std::unique_ptr<ASTNode> left,
             std::unique_ptr<ASTNode> right)
      : ASTNode(line), left(std::move(left)), right(std::move(right)) {}

  ASTNodeType getType() const override { return ASTNodeType::Assign; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Assign (=)\n";
    if (left) {
      left->print(indent + 1);
    }
    if (right) {
      right->print(indent + 1);
    }
  }
};

class BinaryOpNode : public ASTNode {
public:
  TokenType op;
  std::unique_ptr<ASTNode> left;
  std::unique_ptr<ASTNode> right;

  BinaryOpNode(int line, TokenType op, std::unique_ptr<ASTNode> left,
               std::unique_ptr<ASTNode> right)
      : ASTNode(line), op(op), left(std::move(left)), right(std::move(right)) {}

  ASTNodeType getType() const override { return ASTNodeType::BinaryOp; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "BinaryOp (" << token_op_str(op) << ")\n";
    if (left) {
      left->print(indent + 1);
    }
    if (right) {
      right->print(indent + 1);
    }
  }
};

class UnaryOpNode : public ASTNode {
public:
  TokenType op;
  std::unique_ptr<ASTNode> expr;

  UnaryOpNode(int line, TokenType op, std::unique_ptr<ASTNode> expr)
      : ASTNode(line), op(op), expr(std::move(expr)) {}

  ASTNodeType getType() const override { return ASTNodeType::UnaryOp; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "UnaryOp (" << token_op_str(op) << ")\n";
    if (expr) {
      expr->print(indent + 1);
    }
  }
};

class IntLitNode : public ASTNode {
public:
  int64_t val{0};

  IntLitNode(int line, int64_t val) : ASTNode(line), val(val) {}

  ASTNodeType getType() const override { return ASTNodeType::IntLit; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "IntLit: " << val << "\n";
  }
};

class IdentifierNode : public ASTNode {
public:
  std::string name;

  IdentifierNode(int line, std::string name)
      : ASTNode(line), name(std::move(name)) {}

  ASTNodeType getType() const override { return ASTNodeType::Identifier; }
  void accept(ASTVisitor &visitor) const override { visitor.visit(*this); }

  void print(int indent = 0) const override {
    print_indent(indent);
    std::cout << "Ident: " << name << "\n";
  }
};

#endif // AST_HPP
