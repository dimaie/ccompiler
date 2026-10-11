#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

static std::unique_ptr<ProgramNode> parse_source(const char *source) {
  Lexer lexer;
  lexer.init(source);
  Parser parser(&lexer);
  return parser.parse_program();
}

static void test_program_node() {
  std::cout << "Testing ProgramNode...\n";

  // Empty program
  {
    auto prog = parse_source("");
    assert(prog->getType() == ASTNodeType::Program);
    assert(prog->statements.empty());
  }

  // Multiple statements
  {
    auto prog = parse_source("int a = 1;\nint b = 2;\nreturn a + b;\n");
    assert(prog->getType() == ASTNodeType::Program);
    assert(prog->statements.size() == 3);
  }
}

static void test_var_decl_node() {
  std::cout << "Testing VarDeclNode...\n";

  // Declaration without initializer
  {
    auto prog = parse_source("int x;\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::VarDecl);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->name == "x");
    assert(var->init == nullptr);
  }

  // Declaration with literal initializer
  {
    auto prog = parse_source("int count = 42;\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->name == "count");
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::IntLit);
    auto *lit = static_cast<IntLitNode *>(var->init.get());
    assert(lit->val == 42);
  }

  // Declaration with expression initializer
  {
    auto prog = parse_source("int result = a * 2 + 1;\n");
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::BinaryOp);
  }

  // Short declarations
  {
    auto prog = parse_source("short s = 10;\nshort int si = 20;\n");
    assert(prog->statements.size() == 2);
    auto *var1 = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var1->name == "s");
    assert(var1->data_type == DataType::Short);
    auto *var2 = static_cast<VarDeclNode *>(prog->statements[1].get());
    assert(var2->name == "si");
    assert(var2->data_type == DataType::Short);
  }

  // Long and Long Long declarations
  {
    auto prog = parse_source("long l = 100;\nlong int li = 200;\nlong long ll = 300;\nlong long int lli = 400;\n");
    assert(prog->statements.size() == 4);
    auto *var1 = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var1->name == "l");
    assert(var1->data_type == DataType::Long);
    auto *var2 = static_cast<VarDeclNode *>(prog->statements[1].get());
    assert(var2->name == "li");
    assert(var2->data_type == DataType::Long);
    auto *var3 = static_cast<VarDeclNode *>(prog->statements[2].get());
    assert(var3->name == "ll");
    assert(var3->data_type == DataType::LongLong);
    auto *var4 = static_cast<VarDeclNode *>(prog->statements[3].get());
    assert(var4->name == "lli");
    assert(var4->data_type == DataType::LongLong);
  }

  // Short declaration in for loop
  {
    auto prog = parse_source("for (short i = 0; i < 5; i = i + 1) {}\n");
    assert(prog->statements.size() == 1);
    auto *for_node = static_cast<ForNode *>(prog->statements[0].get());
    assert(for_node->init->getType() == ASTNodeType::VarDecl);
    auto *var = static_cast<VarDeclNode *>(for_node->init.get());
    assert(var->name == "i");
    assert(var->data_type == DataType::Short);
  }
}

static void test_if_node() {
  std::cout << "Testing IfNode...\n";

  // If without else
  {
    auto prog = parse_source("if (x > 0) return 1;\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::If);
    auto *if_node = static_cast<IfNode *>(prog->statements[0].get());
    assert(if_node->cond != nullptr);
    assert(if_node->cond->getType() == ASTNodeType::BinaryOp);
    assert(if_node->then_branch != nullptr);
    assert(if_node->then_branch->getType() == ASTNodeType::Return);
    assert(if_node->else_branch == nullptr);
  }

  // If with else
  {
    auto prog = parse_source("if (x == 0) return 1; else return 2;\n");
    auto *if_node = static_cast<IfNode *>(prog->statements[0].get());
    assert(if_node->else_branch != nullptr);
    assert(if_node->else_branch->getType() == ASTNodeType::Return);
  }

  // If with compound statement blocks
  {
    auto prog =
        parse_source("if (flag) { int a = 1; return a; } else { return 0; }\n");
    auto *if_node = static_cast<IfNode *>(prog->statements[0].get());
    assert(if_node->then_branch->getType() == ASTNodeType::Compound);
    assert(if_node->else_branch->getType() == ASTNodeType::Compound);
  }
}

static void test_for_node() {
  std::cout << "Testing ForNode...\n";

  // Full for loop with var_decl init
  {
    auto prog = parse_source("for (int i = 0; i < 10; i = i + 1) return i;\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::For);
    auto *for_node = static_cast<ForNode *>(prog->statements[0].get());
    assert(for_node->init != nullptr &&
           for_node->init->getType() == ASTNodeType::VarDecl);
    assert(for_node->cond != nullptr &&
           for_node->cond->getType() == ASTNodeType::BinaryOp);
    assert(for_node->step != nullptr &&
           for_node->step->getType() == ASTNodeType::Assign);
    assert(for_node->body != nullptr &&
           for_node->body->getType() == ASTNodeType::Return);
  }

  // For loop with expression init
  {
    auto prog = parse_source("for (i = 0; i < 5; i = i + 1) { return i; }\n");
    auto *for_node = static_cast<ForNode *>(prog->statements[0].get());
    assert(for_node->init != nullptr &&
           for_node->init->getType() == ASTNodeType::Assign);
    assert(for_node->body->getType() == ASTNodeType::Compound);
  }

  // For loop with omitted init
  {
    auto prog = parse_source("for (; i < 10; i = i + 1) return 0;\n");
    auto *for_node = static_cast<ForNode *>(prog->statements[0].get());
    assert(for_node->init == nullptr);
    assert(for_node->cond != nullptr);
    assert(for_node->step != nullptr);
  }

  // For loop with omitted cond
  {
    auto prog = parse_source("for (int i = 0; ; i = i + 1) return 0;\n");
    auto *for_node = static_cast<ForNode *>(prog->statements[0].get());
    assert(for_node->init != nullptr);
    assert(for_node->cond == nullptr);
    assert(for_node->step != nullptr);
  }

  // For loop with omitted step
  {
    auto prog = parse_source("for (int i = 0; i < 10; ) return 0;\n");
    auto *for_node = static_cast<ForNode *>(prog->statements[0].get());
    assert(for_node->init != nullptr);
    assert(for_node->cond != nullptr);
    assert(for_node->step == nullptr);
  }

  // Infinite for loop (all omitted)
  {
    auto prog = parse_source("for (;;) { return 0; }\n");
    auto *for_node = static_cast<ForNode *>(prog->statements[0].get());
    assert(for_node->init == nullptr);
    assert(for_node->cond == nullptr);
    assert(for_node->step == nullptr);
    assert(for_node->body->getType() == ASTNodeType::Compound);
  }
}

static void test_while_node() {
  std::cout << "Testing WhileNode...\n";

  // While loop with compound body
  {
    auto prog = parse_source("while (a < 10) { a = a + 1; }\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::While);
    auto *while_node = static_cast<WhileNode *>(prog->statements[0].get());
    assert(while_node->cond != nullptr);
    assert(while_node->cond->getType() == ASTNodeType::BinaryOp);
    assert(while_node->body != nullptr);
    assert(while_node->body->getType() == ASTNodeType::Compound);
  }

  // While loop with single-statement body
  {
    auto prog = parse_source("while (x > 0) return x;\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::While);
    auto *while_node = static_cast<WhileNode *>(prog->statements[0].get());
    assert(while_node->cond != nullptr);
    assert(while_node->cond->getType() == ASTNodeType::BinaryOp);
    assert(while_node->body != nullptr);
    assert(while_node->body->getType() == ASTNodeType::Return);
  }
}

static void test_do_while_node() {
  std::cout << "Testing DoWhileNode...\n";

  // Do-while loop with compound body
  {
    auto prog = parse_source("do { a = a + 1; } while (a < 10);\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::DoWhile);
    auto *do_node = static_cast<DoWhileNode *>(prog->statements[0].get());
    assert(do_node->cond != nullptr);
    assert(do_node->cond->getType() == ASTNodeType::BinaryOp);
    assert(do_node->body != nullptr);
    assert(do_node->body->getType() == ASTNodeType::Compound);
  }

  // Do-while loop with single statement body
  {
    auto prog = parse_source("do a = a + 1; while (a < 5);\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::DoWhile);
    auto *do_node = static_cast<DoWhileNode *>(prog->statements[0].get());
    assert(do_node->cond != nullptr);
    assert(do_node->cond->getType() == ASTNodeType::BinaryOp);
    assert(do_node->body != nullptr);
    assert(do_node->body->getType() == ASTNodeType::Assign);
  }
}

static void test_switch_node() {
  std::cout << "Testing SwitchNode & CaseNode...\n";

  // Standard switch with cases and default
  {
    auto prog = parse_source("switch (x) {\n"
                             "  case 1:\n"
                             "    a = 10;\n"
                             "    break;\n"
                             "  case 2:\n"
                             "    a = 20;\n"
                             "    break;\n"
                             "  default:\n"
                             "    a = 0;\n"
                             "    break;\n"
                             "}\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::Switch);
    auto *sw = static_cast<SwitchNode *>(prog->statements[0].get());
    assert(sw->cond != nullptr && sw->cond->getType() == ASTNodeType::Identifier);
    assert(sw->cases.size() == 3);

    // Case 1
    assert(!sw->cases[0]->is_default);
    assert(sw->cases[0]->value != nullptr && sw->cases[0]->value->getType() == ASTNodeType::IntLit);
    assert(sw->cases[0]->statements.size() == 2);
    assert(sw->cases[0]->statements[0]->getType() == ASTNodeType::Assign);
    assert(sw->cases[0]->statements[1]->getType() == ASTNodeType::Break);

    // Case 2
    assert(!sw->cases[1]->is_default);
    assert(sw->cases[1]->value != nullptr);
    assert(sw->cases[1]->statements.size() == 2);

    // Default case
    assert(sw->cases[2]->is_default);
    assert(sw->cases[2]->value == nullptr);
    assert(sw->cases[2]->statements.size() == 2);
  }

  // Switch with stacked cases
  {
    auto prog = parse_source("switch (x) {\n"
                             "  case 1:\n"
                             "  case 2:\n"
                             "    return 1;\n"
                             "}\n");
    assert(prog->statements.size() == 1);
    auto *sw = static_cast<SwitchNode *>(prog->statements[0].get());
    assert(sw->cases.size() == 2);
    assert(sw->cases[0]->statements.empty());
    assert(sw->cases[1]->statements.size() == 1);
  }
}

static void test_break_node() {
  std::cout << "Testing BreakNode...\n";

  auto prog = parse_source("break;\n");
  assert(prog->statements.size() == 1);
  assert(prog->statements[0]->getType() == ASTNodeType::Break);
}

static void test_continue_node() {
  std::cout << "Testing ContinueNode...\n";

  auto prog = parse_source("continue;\n");
  assert(prog->statements.size() == 1);
  assert(prog->statements[0]->getType() == ASTNodeType::Continue);
}

static void test_goto_node() {
  std::cout << "Testing GotoNode...\n";

  auto prog = parse_source("goto my_label;\n");
  assert(prog->statements.size() == 1);
  assert(prog->statements[0]->getType() == ASTNodeType::Goto);
  auto *g = static_cast<GotoNode *>(prog->statements[0].get());
  assert(g->label == "my_label");
}

static void test_label_node() {
  std::cout << "Testing LabelNode...\n";

  auto prog = parse_source("start:\n"
                           "x = 1;\n"
                           ";\n"
                           "goto start;\n");
  assert(prog->statements.size() == 3);
  assert(prog->statements[0]->getType() == ASTNodeType::Label);
  auto *lbl = static_cast<LabelNode *>(prog->statements[0].get());
  assert(lbl->name == "start");
  assert(prog->statements[1]->getType() == ASTNodeType::Assign);
  assert(prog->statements[2]->getType() == ASTNodeType::Goto);
}

static void test_return_node() {
  std::cout << "Testing ReturnNode...\n";

  // Return literal
  {
    auto prog = parse_source("return 42;\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::Return);
    auto *ret = static_cast<ReturnNode *>(prog->statements[0].get());
    assert(ret->expr != nullptr && ret->expr->getType() == ASTNodeType::IntLit);
    auto *lit = static_cast<IntLitNode *>(ret->expr.get());
    assert(lit->val == 42);
  }

  // Return binary expression
  {
    auto prog = parse_source("return a + b;\n");
    auto *ret = static_cast<ReturnNode *>(prog->statements[0].get());
    assert(ret->expr != nullptr &&
           ret->expr->getType() == ASTNodeType::BinaryOp);
  }
}

static void test_assign_node() {
  std::cout << "Testing AssignNode...\n";

  // Simple assignment
  {
    auto prog = parse_source("x = 10;\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::Assign);
    auto *assign = static_cast<AssignNode *>(prog->statements[0].get());
    assert(assign->left != nullptr &&
           assign->left->getType() == ASTNodeType::Identifier);
    auto *ident = static_cast<IdentifierNode *>(assign->left.get());
    assert(ident->name == "x");
    assert(assign->right != nullptr &&
           assign->right->getType() == ASTNodeType::IntLit);
  }

  // Chained assignment (right-associative: a = (b = 5))
  {
    auto prog = parse_source("a = b = 5;\n");
    auto *assign = static_cast<AssignNode *>(prog->statements[0].get());
    assert(assign->left->getType() == ASTNodeType::Identifier);
    assert(assign->right->getType() == ASTNodeType::Assign);
    auto *inner = static_cast<AssignNode *>(assign->right.get());
    assert(inner->left->getType() == ASTNodeType::Identifier);
    assert(inner->right->getType() == ASTNodeType::IntLit);
  }
}

static void test_binary_op_node() {
  std::cout << "Testing BinaryOpNode...\n";

  // Additive operators
  {
    auto prog = parse_source("a + b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_PLUS);
  }
  {
    auto prog = parse_source("a - b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_MINUS);
  }

  // Multiplicative operators
  {
    auto prog = parse_source("a * b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_STAR);
  }
  {
    auto prog = parse_source("a / b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_SLASH);
  }
  {
    auto prog = parse_source("a % b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_PERCENT);
  }

  // Relational operators
  {
    auto prog = parse_source("a < b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_LESS);
  }
  {
    auto prog = parse_source("a <= b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_LESS_EQ);
  }
  {
    auto prog = parse_source("a > b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_GREATER);
  }
  {
    auto prog = parse_source("a >= b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_GREATER_EQ);
  }

  // Equality operators
  {
    auto prog = parse_source("a == b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_EQUAL);
  }
  {
    auto prog = parse_source("a != b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_NOT_EQUAL);
  }

  // Logical operators
  {
    auto prog = parse_source("a && b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_LOGIC_AND);
  }
  {
    auto prog = parse_source("a || b;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_LOGIC_OR);
  }

  // Precedence: 1 + 2 * 3 -> root is '+'
  {
    auto prog = parse_source("1 + 2 * 3;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_PLUS);
    assert(bin->left->getType() == ASTNodeType::IntLit);
    assert(bin->right->getType() == ASTNodeType::BinaryOp);
    auto *right = static_cast<BinaryOpNode *>(bin->right.get());
    assert(right->op == TOK_STAR);
  }

  // Parenthesized grouping: (1 + 2) * 3 -> root is '*'
  {
    auto prog = parse_source("(1 + 2) * 3;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_STAR);
    assert(bin->left->getType() == ASTNodeType::BinaryOp);
    assert(bin->right->getType() == ASTNodeType::IntLit);
  }

  // Left associativity: 1 - 2 - 3 -> (1 - 2) - 3
  {
    auto prog = parse_source("1 - 2 - 3;\n");
    auto *bin = static_cast<BinaryOpNode *>(prog->statements[0].get());
    assert(bin->op == TOK_MINUS);
    assert(bin->left->getType() == ASTNodeType::BinaryOp);
    assert(bin->right->getType() == ASTNodeType::IntLit);
  }
}

static void test_unary_op_node() {
  std::cout << "Testing UnaryOpNode...\n";

  // Positive: +x
  {
    auto prog = parse_source("+x;\n");
    auto *un = static_cast<UnaryOpNode *>(prog->statements[0].get());
    assert(un->op == TOK_PLUS);
    assert(un->expr->getType() == ASTNodeType::Identifier);
  }

  // Negation: -x
  {
    auto prog = parse_source("-x;\n");
    auto *un = static_cast<UnaryOpNode *>(prog->statements[0].get());
    assert(un->op == TOK_MINUS);
  }

  // Logical NOT: !x
  {
    auto prog = parse_source("!x;\n");
    auto *un = static_cast<UnaryOpNode *>(prog->statements[0].get());
    assert(un->op == TOK_LOGIC_NOT);
  }

  // Bitwise NOT: ~x
  {
    auto prog = parse_source("~x;\n");
    auto *un = static_cast<UnaryOpNode *>(prog->statements[0].get());
    assert(un->op == TOK_TILDE);
  }

  // Chained unaries: !-~x
  {
    auto prog = parse_source("!-~x;\n");
    auto *un1 = static_cast<UnaryOpNode *>(prog->statements[0].get());
    assert(un1->op == TOK_LOGIC_NOT);
    assert(un1->expr->getType() == ASTNodeType::UnaryOp);
    auto *un2 = static_cast<UnaryOpNode *>(un1->expr.get());
    assert(un2->op == TOK_MINUS);
    assert(un2->expr->getType() == ASTNodeType::UnaryOp);
    auto *un3 = static_cast<UnaryOpNode *>(un2->expr.get());
    assert(un3->op == TOK_TILDE);
    assert(un3->expr->getType() == ASTNodeType::Identifier);
  }
}

static void test_int_lit_node() {
  std::cout << "Testing IntLitNode...\n";

  // Decimal
  {
    auto prog = parse_source("12345;\n");
    auto *lit = static_cast<IntLitNode *>(prog->statements[0].get());
    assert(lit->val == 12345);
  }

  // Hexadecimal
  {
    auto prog = parse_source("0x2A;\n"); // 42 in decimal
    auto *lit = static_cast<IntLitNode *>(prog->statements[0].get());
    assert(lit->val == 42);
  }

  // Zero
  {
    auto prog = parse_source("0;\n");
    auto *lit = static_cast<IntLitNode *>(prog->statements[0].get());
    assert(lit->val == 0);
  }
}

static void test_identifier_node() {
  std::cout << "Testing IdentifierNode...\n";

  {
    auto prog = parse_source("variable_name_123;\n");
    auto *ident = static_cast<IdentifierNode *>(prog->statements[0].get());
    assert(ident->name == "variable_name_123");
  }
}

static void test_compound_node() {
  std::cout << "Testing CompoundNode...\n";

  // Empty compound block
  {
    auto prog = parse_source("{}\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::Compound);
    auto *block = static_cast<CompoundNode *>(prog->statements[0].get());
    assert(block->statements.empty());
  }

  // Nested compound block with multiple statements
  {
    auto prog =
        parse_source("{\n  int x = 1;\n  {\n    x = 2;\n  }\n  return x;\n}\n");
    auto *block = static_cast<CompoundNode *>(prog->statements[0].get());
    assert(block->statements.size() == 3);
    assert(block->statements[1]->getType() == ASTNodeType::Compound);
  }
}

static void test_syntax_errors() {
  std::cout << "Testing Syntax Error Exceptions...\n";

  // Missing semicolon
  {
    bool threw = false;
    try {
      parse_source("int x = 5\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Assignment to non-identifier
  {
    bool threw = false;
    try {
      parse_source("5 = 10;\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Unclosed parenthesis
  {
    bool threw = false;
    try {
      parse_source("(1 + 2;\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Missing '(' after while
  {
    bool threw = false;
    try {
      parse_source("while x > 0) return 1;\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Missing 'while' after do body
  {
    bool threw = false;
    try {
      parse_source("do { x = 1; } (x > 0);\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Missing ';' after do-while
  {
    bool threw = false;
    try {
      parse_source("do { x = 1; } while (x > 0)\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Missing ':' after case value
  {
    bool threw = false;
    try {
      parse_source("switch (x) { case 1 a = 1; }\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Missing ';' after break
  {
    bool threw = false;
    try {
      parse_source("break\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Missing ';' after continue
  {
    bool threw = false;
    try {
      parse_source("continue\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Missing label identifier after goto
  {
    bool threw = false;
    try {
      parse_source("goto ;\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Missing ';' after goto label
  {
    bool threw = false;
    try {
      parse_source("goto my_label\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }
}

static void test_token_names() {
  std::cout << "Testing token_name()...\n";

  assert(std::strcmp(token_name(TOK_EOF), "EOF") == 0);
  assert(std::strcmp(token_name(TOK_IDENTIFIER), "IDENTIFIER") == 0);
  assert(std::strcmp(token_name(TOK_COLON), ":") == 0);
  assert(std::strcmp(token_name(TOK_SEMICOLON), ";") == 0);

  // Keywords from KEYWORDS map
  assert(std::strcmp(token_name(TOK_KW_IF), "if") == 0);
  assert(std::strcmp(token_name(TOK_KW_ELSE), "else") == 0);
  assert(std::strcmp(token_name(TOK_KW_WHILE), "while") == 0);
  assert(std::strcmp(token_name(TOK_KW_FOR), "for") == 0);
  assert(std::strcmp(token_name(TOK_KW_DO), "do") == 0);
  assert(std::strcmp(token_name(TOK_KW_SWITCH), "switch") == 0);
  assert(std::strcmp(token_name(TOK_KW_CASE), "case") == 0);
  assert(std::strcmp(token_name(TOK_KW_BREAK), "break") == 0);
  assert(std::strcmp(token_name(TOK_KW_CONTINUE), "continue") == 0);
  assert(std::strcmp(token_name(TOK_KW_GOTO), "goto") == 0);
  assert(std::strcmp(token_name(TOK_KW_RETURN), "return") == 0);
}

static void test_lexer_peek() {
  std::cout << "Testing Lexer::peek(k)...\n";

  Lexer lexer;
  lexer.init("label: return 42;");

  // Default peek() is k=1
  assert(lexer.peek().type == TOK_IDENTIFIER);
  assert(std::strcmp(lexer.peek().text, "label") == 0);

  // peek(1) matches peek()
  assert(lexer.peek(1).type == TOK_IDENTIFIER);
  assert(std::strcmp(lexer.peek(1).text, "label") == 0);

  // peek(2) looks ahead to colon
  assert(lexer.peek(2).type == TOK_COLON);
  assert(std::strcmp(lexer.peek(2).text, ":") == 0);

  // Advancing consumes token
  Token t1 = lexer.next();
  assert(t1.type == TOK_IDENTIFIER);

  // Now peek() is colon, peek(2) is return keyword
  assert(lexer.peek().type == TOK_COLON);
  assert(lexer.peek(2).type == TOK_KW_RETURN);

  // Test lookahead depth up to LOOKAHEAD_BUFFER (4)
  {
    Lexer lex;
    lex.init("a + b * c;");
    assert(lex.peek(1).type == TOK_IDENTIFIER && std::strcmp(lex.peek(1).text, "a") == 0);
    assert(lex.peek(2).type == TOK_PLUS);
    assert(lex.peek(3).type == TOK_IDENTIFIER && std::strcmp(lex.peek(3).text, "b") == 0);
    assert(lex.peek(4).type == TOK_STAR);

    // Consume 'a'
    assert(lex.next().type == TOK_IDENTIFIER);
    // Now peek(1) is '+', peek(2) is 'b', peek(3) is '*'
    assert(lex.peek(1).type == TOK_PLUS);
    assert(lex.peek(2).type == TOK_IDENTIFIER && std::strcmp(lex.peek(2).text, "b") == 0);
    assert(lex.peek(3).type == TOK_STAR);

    // Consume '+'
    assert(lex.next().type == TOK_PLUS);
    assert(lex.peek(1).type == TOK_IDENTIFIER && std::strcmp(lex.peek(1).text, "b") == 0);
    assert(lex.peek(2).type == TOK_STAR);
  }
}

static void test_func_def_node() {
  std::cout << "Testing FuncDefNode...\n";

  // Function with no params
  {
    auto prog = parse_source("int foo() { return 1; }\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::FuncDef);
    auto *func = static_cast<FuncDefNode *>(prog->statements[0].get());
    assert(func->return_type == DataType::Int);
    assert(func->name == "foo");
    assert(func->params.empty());
    assert(func->body != nullptr);
    assert(func->body->statements.size() == 1);
  }

  // Function with (void) params
  {
    auto prog = parse_source("int bar(void) { return 2; }\n");
    assert(prog->statements.size() == 1);
    auto *func = static_cast<FuncDefNode *>(prog->statements[0].get());
    assert(func->name == "bar");
    assert(func->params.empty());
  }

  // Void function with empty return
  {
    auto prog = parse_source("void baz() { return; }\n");
    assert(prog->statements.size() == 1);
    auto *func = static_cast<FuncDefNode *>(prog->statements[0].get());
    assert(func->return_type == DataType::Void);
    assert(func->name == "baz");
    assert(func->params.empty());
    assert(func->body->statements.size() == 1);
    assert(func->body->statements[0]->getType() == ASTNodeType::Return);
    auto *ret = static_cast<ReturnNode *>(func->body->statements[0].get());
    assert(ret->expr == nullptr);
  }

  // Function with multiple parameters and various types
  {
    auto prog = parse_source("long add(int a, short b, long c) { return a + b + c; }\n");
    assert(prog->statements.size() == 1);
    auto *func = static_cast<FuncDefNode *>(prog->statements[0].get());
    assert(func->return_type == DataType::Long);
    assert(func->name == "add");
    assert(func->params.size() == 3);
    assert(func->params[0].type == DataType::Int && func->params[0].name == "a");
    assert(func->params[1].type == DataType::Short && func->params[1].name == "b");
    assert(func->params[2].type == DataType::Long && func->params[2].name == "c");
  }

  // Error: Parameter of type void
  {
    bool threw = false;
    try {
      parse_source("int bad(void x) { return 0; }\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }

  // Error: Variable of type void
  {
    bool threw = false;
    try {
      parse_source("void bad_var;\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }
}

static void test_call_node() {
  std::cout << "Testing CallNode...\n";

  // Call with 0 args
  {
    auto prog = parse_source("int x = foo();\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::Call);
    auto *call = static_cast<CallNode *>(var->init.get());
    assert(call->callee == "foo");
    assert(call->args.empty());
  }

  // Call with multiple args and expressions
  {
    auto prog = parse_source("return add(1, 2 + 3, foo());\n");
    assert(prog->statements.size() == 1);
    auto *ret = static_cast<ReturnNode *>(prog->statements[0].get());
    assert(ret->expr->getType() == ASTNodeType::Call);
    auto *call = static_cast<CallNode *>(ret->expr.get());
    assert(call->callee == "add");
    assert(call->args.size() == 3);
    assert(call->args[0]->getType() == ASTNodeType::IntLit);
    assert(call->args[1]->getType() == ASTNodeType::BinaryOp);
    assert(call->args[2]->getType() == ASTNodeType::Call);
  }
}

static void test_array_node() {
  std::cout << "Testing Array Declarations & Subscripting...\n";

  // Declaration without initializer
  {
    auto prog = parse_source("int arr[5];\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::VarDecl);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->name == "arr");
    assert(var->is_array == true);
    assert(var->array_size == 5);
    assert(var->init == nullptr);
  }

  // Declaration with initializer list
  {
    auto prog = parse_source("int a[3] = {1, 2, 3};\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->name == "a");
    assert(var->is_array == true);
    assert(var->array_size == 3);
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::InitList);
    auto *init_list = static_cast<InitListNode *>(var->init.get());
    assert(init_list->elements.size() == 3);
  }

  // Declaration with inferred size
  {
    auto prog = parse_source("int a[] = {10, 20, 30, 40};\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->name == "a");
    assert(var->is_array == true);
    assert(var->array_size == 4);
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::InitList);
  }

  // Initializer list with trailing comma
  {
    auto prog = parse_source("short s[2] = {100, 200,};\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->data_type == DataType::Short);
    assert(var->is_array == true);
    assert(var->array_size == 2);
    auto *init_list = static_cast<InitListNode *>(var->init.get());
    assert(init_list->elements.size() == 2);
  }

  // Array access in expression
  {
    auto prog = parse_source("int x = a[0] + a[i + 1];\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::BinaryOp);
    auto *bin = static_cast<BinaryOpNode *>(var->init.get());
    assert(bin->left->getType() == ASTNodeType::ArrayAccess);
    assert(bin->right->getType() == ASTNodeType::ArrayAccess);
    auto *acc = static_cast<ArrayAccessNode *>(bin->left.get());
    assert(acc->array->getType() == ASTNodeType::Identifier);
    auto *arr_id = static_cast<IdentifierNode *>(acc->array.get());
    assert(arr_id->name == "a");
    assert(acc->index->getType() == ASTNodeType::IntLit);
  }

  // Assignment to array element
  {
    auto prog = parse_source("a[2] = 42;\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::Assign);
    auto *assign = static_cast<AssignNode *>(prog->statements[0].get());
    assert(assign->left->getType() == ASTNodeType::ArrayAccess);
    auto *acc = static_cast<ArrayAccessNode *>(assign->left.get());
    auto *arr_id = static_cast<IdentifierNode *>(acc->array.get());
    assert(arr_id->name == "a");
    assert(acc->index->getType() == ASTNodeType::IntLit);
    assert(assign->right->getType() == ASTNodeType::IntLit);
  }

  // Error handling: size 0
  {
    bool threw = false;
    try {
      parse_source("int a[0];\n");
    } catch (const std::runtime_error &) {
      threw = true;
    }
    assert(threw);
  }

  // Error handling: missing size without initializer
  {
    bool threw = false;
    try {
      parse_source("int a[];\n");
    } catch (const std::runtime_error &) {
      threw = true;
    }
    assert(threw);
  }

  // Error handling: excess initializers
  {
    bool threw = false;
    try {
      parse_source("int a[2] = {1, 2, 3};\n");
    } catch (const std::runtime_error &) {
      threw = true;
    }
    assert(threw);
  }

  // Error handling: array initialized with non-list
  {
    bool threw = false;
    try {
      parse_source("int a[2] = 5;\n");
    } catch (const std::runtime_error &) {
      threw = true;
    }
    assert(threw);
  }

  // Error handling: scalar initialized with list
  {
    bool threw = false;
    try {
      parse_source("int a = {1, 2};\n");
    } catch (const std::runtime_error &) {
      threw = true;
    }
    assert(threw);
  }
}

static void test_pointer_node() {
  std::cout << "Testing Pointers (Declarations, Address-of, Dereference)...\n";

  // Pointer variable declarations
  {
    auto prog = parse_source("int *p;\nint **pp;\nshort *sp;\nvoid *vp;\n");
    assert(prog->statements.size() == 4);

    auto *p = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(p->name == "p");
    assert(p->pointer_level == 1);
    assert(p->data_type == DataType::Int);

    auto *pp = static_cast<VarDeclNode *>(prog->statements[1].get());
    assert(pp->name == "pp");
    assert(pp->pointer_level == 2);
    assert(pp->data_type == DataType::Int);

    auto *sp = static_cast<VarDeclNode *>(prog->statements[2].get());
    assert(sp->name == "sp");
    assert(sp->pointer_level == 1);
    assert(sp->data_type == DataType::Short);

    auto *vp = static_cast<VarDeclNode *>(prog->statements[3].get());
    assert(vp->name == "vp");
    assert(vp->pointer_level == 1);
    assert(vp->data_type == DataType::Void);
  }

  // Pointer initialization with address-of
  {
    auto prog = parse_source("int x = 10;\nint *p = &x;\n");
    assert(prog->statements.size() == 2);
    auto *var = static_cast<VarDeclNode *>(prog->statements[1].get());
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::UnaryOp);
    auto *unary = static_cast<UnaryOpNode *>(var->init.get());
    assert(unary->op == TOK_AMP);
    assert(unary->expr->getType() == ASTNodeType::Identifier);
  }

  // Dereference read and dereference assignment
  {
    auto prog = parse_source("*p = 42;\nint y = *p + 1;\n");
    assert(prog->statements.size() == 2);

    auto *assign = static_cast<AssignNode *>(prog->statements[0].get());
    assert(assign->left->getType() == ASTNodeType::UnaryOp);
    auto *lhs_deref = static_cast<UnaryOpNode *>(assign->left.get());
    assert(lhs_deref->op == TOK_STAR);

    auto *var_y = static_cast<VarDeclNode *>(prog->statements[1].get());
    assert(var_y->init->getType() == ASTNodeType::BinaryOp);
  }

  // Function with pointer parameters and return type
  {
    auto prog = parse_source("int *get_ptr(int *a, int **b) {\nreturn a;\n}\n");
    assert(prog->statements.size() == 1);
    auto *fn = static_cast<FuncDefNode *>(prog->statements[0].get());
    assert(fn->name == "get_ptr");
    assert(fn->return_type == DataType::Int);
    assert(fn->return_pointer_level == 1);
    assert(fn->params.size() == 2);
    assert(fn->params[0].pointer_level == 1);
    assert(fn->params[1].pointer_level == 2);
  }
}

static void test_func_ptr_node() {
  std::cout << "Testing Function Pointers (Declarations and Calls)...\n";

  // Function pointer declaration without init
  {
    auto prog = parse_source("int (*fp)(int, int);\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->name == "fp");
    assert(var->is_func_ptr == true);
    assert(var->func_ptr_ret_type == DataType::Int);
    assert(var->func_ptr_params.size() == 2);
    assert(var->init == nullptr);
  }

  // Function pointer declaration with direct symbol initialization
  {
    auto prog = parse_source("int (*fp)(int, int) = add;\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->is_func_ptr == true);
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::Identifier);
    auto *id = static_cast<IdentifierNode *>(var->init.get());
    assert(id->name == "add");
  }

  // Function pointer declaration with & symbol initialization
  {
    auto prog = parse_source("int (*fp)(int, int) = &add;\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->is_func_ptr == true);
    assert(var->init != nullptr);
    assert(var->init->getType() == ASTNodeType::UnaryOp);
    auto *unary = static_cast<UnaryOpNode *>(var->init.get());
    assert(unary->op == TOK_AMP);
  }

  // Indirect calls: (*fp)(1, 2)
  {
    auto prog = parse_source("int res = (*fp)(1, 2);\n");
    assert(prog->statements.size() == 1);
    auto *var = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(var->init->getType() == ASTNodeType::Call);
    auto *call = static_cast<CallNode *>(var->init.get());
    assert(call->callee_expr != nullptr);
    assert(call->callee_expr->getType() == ASTNodeType::UnaryOp);
    assert(call->args.size() == 2);
  }

  // Function parameter taking function pointer
  {
    auto prog = parse_source("int apply(int (*op)(int, int), int x, int y) {\nreturn op(x, y);\n}\n");
    assert(prog->statements.size() == 1);
    auto *fn = static_cast<FuncDefNode *>(prog->statements[0].get());
    assert(fn->params.size() == 3);
    assert(fn->params[0].is_func_ptr == true);
    assert(fn->params[0].name == "op");
    assert(fn->params[0].func_ptr_params.size() == 2);
  }
}

static void test_struct_node() {
  std::cout << "Testing Struct & Union Nodes (Definitions, Declarations, Member Access)...\n";

  // 1. Standalone struct definition
  {
    auto prog = parse_source("struct Point { int x; int y; };\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::StructDef);
    auto *st = static_cast<StructDefNode *>(prog->statements[0].get());
    assert(st->name == "Point");
    assert(st->is_union == false);
    assert(st->members.size() == 2);
    assert(st->members[0].name == "x");
    assert(st->members[1].name == "y");
    assert(st->var_decl == nullptr);
  }

  // 2. Struct variable declaration with initializer
  {
    auto prog = parse_source("struct Point p = {10, 20};\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::VarDecl);
    auto *vd = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(vd->name == "p");
    assert(vd->data_type == DataType::Struct);
    assert(vd->struct_name == "Point");
    assert(vd->init != nullptr);
    assert(vd->init->getType() == ASTNodeType::InitList);
    auto *il = static_cast<InitListNode *>(vd->init.get());
    assert(il->elements.size() == 2);
  }

  // 3. Member access (. and ->) and assignment
  {
    auto prog = parse_source(
        "struct Point p;\n"
        "struct Point *pp = &p;\n"
        "p.x = 42;\n"
        "pp->y = 99;\n"
        "int sum = p.x + pp->y;\n"
        "return sum;\n");
    assert(prog->statements.size() == 6);
    // p.x = 42;
    auto *as1 = static_cast<AssignNode *>(prog->statements[2].get());
    assert(as1->left->getType() == ASTNodeType::MemberAccess);
    auto *m1 = static_cast<MemberAccessNode *>(as1->left.get());
    assert(m1->member == "x");
    assert(m1->is_arrow == false);

    // pp->y = 99;
    auto *as2 = static_cast<AssignNode *>(prog->statements[3].get());
    assert(as2->left->getType() == ASTNodeType::MemberAccess);
    auto *m2 = static_cast<MemberAccessNode *>(as2->left.get());
    assert(m2->member == "y");
    assert(m2->is_arrow == true);
  }

  // 4. Union definition and access
  {
    auto prog = parse_source(
        "union Value {\n"
        "  int i;\n"
        "  short s;\n"
        "};\n"
        "union Value v;\n"
        "v.i = 100;\n"
        "return v.i;\n");
    assert(prog->statements.size() == 4);
    assert(prog->statements[0]->getType() == ASTNodeType::StructDef);
    auto *ud = static_cast<StructDefNode *>(prog->statements[0].get());
    assert(ud->name == "Value");
    assert(ud->is_union == true);
    assert(ud->members.size() == 2);
  }

  // 5. Combined struct definition with variable declaration
  {
    auto prog = parse_source("struct Point { int x; int y; } p;\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::StructDef);
    auto *st = static_cast<StructDefNode *>(prog->statements[0].get());
    assert(st->name == "Point");
    assert(st->var_decl != nullptr);
    assert(st->var_decl->name == "p");
  }
}

static void test_code_review_fixes() {
  std::cout << "Testing Code Review fixes for Lexer and Parser...\n";

  // 1. Lexer: long identifier (> 127 chars) is not split into two tokens
  {
    std::string long_id(200, 'a');
    std::string src = long_id + ";";
    Lexer lexer(src);
    Token tok1 = lexer.next();
    assert(tok1.type == TOK_IDENTIFIER);
    assert(std::strlen(tok1.text) == 127);
    Token tok2 = lexer.next();
    assert(tok2.type == TOK_SEMICOLON); // NOT an identifier continuation!
  }

  // 2. Lexer: non-null-terminated string_view
  {
    std::string full = "int x = 10; float y = 20;";
    std::string_view slice(full.data() + 4, 6); // "x = 10"
    Lexer lexer(slice);
    Token t1 = lexer.next();
    assert(t1.type == TOK_IDENTIFIER);
    assert(std::string(t1.text) == "x");
    Token t2 = lexer.next();
    assert(t2.type == TOK_ASSIGN);
    Token t3 = lexer.next();
    assert(t3.type == TOK_INT_LIT);
    assert(t3.value.int_val == 10);
    Token t4 = lexer.next();
    assert(t4.type == TOK_EOF);
  }

  // 3. Lexer: unterminated string/char literals
  {
    Lexer l1("\"unterminated string");
    Token t1 = l1.next();
    assert(t1.type == TOK_UNKNOWN);

    Lexer l2("'a");
    Token t2 = l2.next();
    assert(t2.type == TOK_UNKNOWN);
  }

  // 4. Lexer: peek(k) bounds check throws std::out_of_range
  {
    Lexer lexer("int a = 1;");
    bool threw = false;
    try {
      lexer.peek(5);
    } catch (const std::out_of_range &) {
      threw = true;
    }
    assert(threw);

    threw = false;
    try {
      lexer.peek(0);
    } catch (const std::out_of_range &) {
      threw = true;
    }
    assert(threw);
  }

  // 5. Lexer: hex and extra escape sequences
  {
    Lexer lexer("'\\x41' '\\a' '\\b' '\\f' '\\v'");
    Token t1 = lexer.next();
    assert(t1.type == TOK_CHAR_LIT);
    assert(t1.value.int_val == 'A');
    Token t2 = lexer.next();
    assert(t2.type == TOK_CHAR_LIT);
    assert(t2.value.int_val == '\a');
    Token t3 = lexer.next();
    assert(t3.type == TOK_CHAR_LIT);
    assert(t3.value.int_val == '\b');
    Token t4 = lexer.next();
    assert(t4.type == TOK_CHAR_LIT);
    assert(t4.value.int_val == '\f');
    Token t5 = lexer.next();
    assert(t5.type == TOK_CHAR_LIT);
    assert(t5.value.int_val == '\v');
  }

  // 6. Parser: Bitwise operators
  {
    auto prog = parse_source("int x = (1 << 2) | (3 >> 1) ^ (4 & 5);\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::VarDecl);
  }

  // 7. Parser: Char literals in expressions
  {
    auto prog = parse_source("int ch = 'Z';\n");
    assert(prog->statements.size() == 1);
    auto *vd = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(vd->init != nullptr);
    assert(vd->init->getType() == ASTNodeType::IntLit);
    assert(static_cast<IntLitNode *>(vd->init.get())->val == 'Z');
  }

  // 8. Parser: Invalid lvalue in assignment reports error
  {
    bool threw = false;
    try {
      parse_source("10 = 20;\n");
    } catch (const ParseException &) {
      threw = true;
    }
    assert(threw);
  }
}

static void test_char_and_string_literals() {
  std::cout << "Testing Char and String Literals...\n";

  // 1. char declaration with char literal
  {
    auto prog = parse_source("char c = 'A';\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::VarDecl);
    auto *vd = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(vd->data_type == DataType::Char);
    assert(vd->name == "c");
    assert(vd->init != nullptr);
    assert(vd->init->getType() == ASTNodeType::IntLit);
    assert(static_cast<IntLitNode *>(vd->init.get())->val == 'A');
  }

  // 2. char pointer with string literal
  {
    auto prog = parse_source("char *msg = \"Hello, World!\";\n");
    assert(prog->statements.size() == 1);
    auto *vd = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(vd->data_type == DataType::Char);
    assert(vd->pointer_level == 1);
    assert(vd->name == "msg");
    assert(vd->init != nullptr);
    assert(vd->init->getType() == ASTNodeType::StringLit);
    assert(static_cast<StringLitNode *>(vd->init.get())->value == "Hello, World!");
  }

  // 3. char array
  {
    auto prog = parse_source("char arr[20];\n");
    assert(prog->statements.size() == 1);
    auto *vd = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(vd->data_type == DataType::Char);
    assert(vd->is_array == true);
    assert(vd->array_size == 20);
  }

  // 4. string literal concatenation
  {
    auto prog = parse_source("char *s = \"Hello, \" \"World!\";\n");
    assert(prog->statements.size() == 1);
    auto *vd = static_cast<VarDeclNode *>(prog->statements[0].get());
    assert(vd->init->getType() == ASTNodeType::StringLit);
    assert(static_cast<StringLitNode *>(vd->init.get())->value == "Hello, World!");
  }

  // 5. function call with string literal
  {
    auto prog = parse_source("int main() { puts(\"test\"); return 0; }\n");
    assert(prog->statements.size() == 1);
    auto *fn = static_cast<FuncDefNode *>(prog->statements[0].get());
    assert(fn->body != nullptr);
    auto *cp = static_cast<CompoundNode *>(fn->body.get());
    assert(cp->statements[0]->getType() == ASTNodeType::Call);
    auto *call = static_cast<CallNode *>(cp->statements[0].get());
    assert(call->callee == "puts");
    assert(call->args.size() == 1);
    assert(call->args[0]->getType() == ASTNodeType::StringLit);
    assert(static_cast<StringLitNode *>(call->args[0].get())->value == "test");
  }
}

#include "preprocessor.hpp"

static void test_preprocessor() {
  std::cout << "Testing Preprocessor Subsystem...\n";

  // 1. Object-like macro definition and expansion
  {
    Preprocessor pp;
    std::string src =
        "#define FOO 42\n"
        "#define BAR (FOO + 1)\n"
        "int x = BAR;\n";
    std::string out = pp.process(src);
    assert(out.find("int x = (42 + 1);") != std::string::npos);
  }

  // 2. Function-like macro definition and expansion
  {
    Preprocessor pp;
    std::string src =
        "#define ADD(a, b) ((a) + (b))\n"
        "#define MULT(x, y) ((x) * (y))\n"
        "int val = ADD(10, MULT(2, 3));\n";
    std::string out = pp.process(src);
    assert(out.find("int val = ((10) + (((2) * (3))));") != std::string::npos);
  }

  // 3. ifdef / ifndef / else / endif
  {
    Preprocessor pp;
    std::string src =
        "#define FLAG_A\n"
        "#ifdef FLAG_A\n"
        "int a = 1;\n"
        "#else\n"
        "int a = 0;\n"
        "#endif\n"
        "#ifndef FLAG_B\n"
        "int b = 2;\n"
        "#else\n"
        "int b = 0;\n"
        "#endif\n";
    std::string out = pp.process(src);
    assert(out.find("int a = 1;") != std::string::npos);
    assert(out.find("int a = 0;") == std::string::npos);
    assert(out.find("int b = 2;") != std::string::npos);
    assert(out.find("int b = 0;") == std::string::npos);
  }

  // 4. Nested conditionals and #undef
  {
    Preprocessor pp;
    std::string src =
        "#define FEATURE\n"
        "#ifdef FEATURE\n"
        "  #define SUBFEATURE 100\n"
        "  int f = SUBFEATURE;\n"
        "  #undef FEATURE\n"
        "#endif\n"
        "#ifdef FEATURE\n"
        "  int error = 1;\n"
        "#else\n"
        "  int clean = 2;\n"
        "#endif\n";
    std::string out = pp.process(src);
    assert(out.find("int f = 100;") != std::string::npos);
    assert(out.find("int error = 1;") == std::string::npos);
    assert(out.find("int clean = 2;") != std::string::npos);
  }

  // 5. #if with defined() and logical expressions
  {
    Preprocessor pp;
    std::string src =
        "#define VERSION 2\n"
        "#if defined(VERSION) && VERSION == 2\n"
        "int v = 2;\n"
        "#elif VERSION == 1\n"
        "int v = 1;\n"
        "#else\n"
        "int v = 0;\n"
        "#endif\n";
    std::string out = pp.process(src);
    assert(out.find("int v = 2;") != std::string::npos);
    assert(out.find("int v = 1;") == std::string::npos);
    assert(out.find("int v = 0;") == std::string::npos);
  }

  // 6. Bundled virtual headers: #include <stdio.h>
  {
    Preprocessor pp;
    std::string src =
        "#include <stdio.h>\n"
        "int main() {\n"
        "    puts(\"test\");\n"
        "    return 0;\n"
        "}\n";
    std::string out = pp.process(src);
    assert(out.find("int puts(const char *s);") != std::string::npos);
    assert(out.find("puts(\"test\");") != std::string::npos);
  }

  // 7. Line continuation with backslash
  {
    Preprocessor pp;
    std::string src =
        "#define MULTILINE(x) \\\n"
        "    int res = (x) * 2; \\\n"
        "    return res;\n"
        "MULTILINE(5)\n";
    std::string out = pp.process(src);
    assert(out.find("int res = (5) * 2;") != std::string::npos);
    assert(out.find("return res;") != std::string::npos);
  }
}

static void test_inline_asm() {
  std::cout << "Testing Inline Assembly Parsing...\n";

  // 1. Basic asm statement
  {
    auto prog = parse_source("asm(\"nop\");\n");
    assert(prog->statements.size() == 1);
    assert(prog->statements[0]->getType() == ASTNodeType::AsmStmt);
    auto *asm_node = static_cast<AsmStmtNode *>(prog->statements[0].get());
    assert(asm_node->asm_code == "nop");
    assert(!asm_node->is_volatile);
  }

  // 2. __asm__ volatile ("...")
  {
    auto prog = parse_source("__asm__ volatile (\"int3\");\n");
    assert(prog->statements.size() == 1);
    auto *asm_node = static_cast<AsmStmtNode *>(prog->statements[0].get());
    assert(asm_node->asm_code == "int3");
    assert(asm_node->is_volatile);
  }

  // 3. String concatenation in asm
  {
    auto prog = parse_source("asm(\"nop\\n\" \"nop\");\n");
    assert(prog->statements.size() == 1);
    auto *asm_node = static_cast<AsmStmtNode *>(prog->statements[0].get());
    assert(asm_node->asm_code.find("nop") != std::string::npos);
  }

  // 4. Extended asm with operands skipped/handled
  {
    auto prog = parse_source("asm(\"movq %0, %rax\" : \"=r\" (val));\n");
    assert(prog->statements.size() == 1);
    auto *asm_node = static_cast<AsmStmtNode *>(prog->statements[0].get());
    assert(asm_node->asm_code == "movq %0, %rax");
  }
}

int main() {
  std::cout << "========================================\n";
  std::cout << "  Running AST & Parser Test Suite\n";
  std::cout << "========================================\n";

  test_program_node();
  test_var_decl_node();
  test_if_node();
  test_for_node();
  test_while_node();
  test_do_while_node();
  test_switch_node();
  test_break_node();
  test_continue_node();
  test_goto_node();
  test_label_node();
  test_return_node();
  test_assign_node();
  test_binary_op_node();
  test_unary_op_node();
  test_int_lit_node();
  test_identifier_node();
  test_compound_node();
  test_syntax_errors();
  test_token_names();
  test_lexer_peek();
  test_func_def_node();
  test_call_node();
  test_array_node();
  test_pointer_node();
  test_func_ptr_node();
  test_struct_node();
  test_code_review_fixes();
  test_char_and_string_literals();
  test_preprocessor();
  test_inline_asm();

  std::cout << "========================================\n";
  std::cout << "  ALL TESTS PASSED SUCCESSFULLY! (31/31)\n";
  std::cout << "========================================\n";
  return 0;
}


