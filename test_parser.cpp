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

  std::cout << "========================================\n";
  std::cout << "  ALL TESTS PASSED SUCCESSFULLY! (21/21)\n";
  std::cout << "========================================\n";
  return 0;
}

