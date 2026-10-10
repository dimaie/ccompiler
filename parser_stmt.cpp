#include "parser.hpp"
#include <utility>

std::unique_ptr<CompoundNode> Parser::parse_compound_statement() {
  Token t = expect(TOK_LBRACE, "Expected '{'");
  auto compound = std::make_unique<CompoundNode>(t.line);

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    compound->append(parse_statement());
  }
  expect(TOK_RBRACE, "Expected '}' after block");
  return compound;
}

std::unique_ptr<IfNode> Parser::parse_if() {
  Token t = expect(TOK_KW_IF, "Expected 'if'");
  auto cond = parse_parenthesized([this] { return parse_expr(); },
                                  "Expected '(' after 'if'",
                                  "Expected ')' after condition");

  auto then_branch = parse_statement();
  std::unique_ptr<ASTNode> else_branch = nullptr;
  if (match(TOK_KW_ELSE)) {
    else_branch = parse_statement();
  }
  return std::make_unique<IfNode>(t.line, std::move(cond),
                                  std::move(then_branch),
                                  std::move(else_branch));
}

std::unique_ptr<ForNode> Parser::parse_for() {
  Token t = expect(TOK_KW_FOR, "Expected 'for'");
  expect(TOK_LPAREN, "Expected '(' after 'for'");

  // 1. Init: var_decl | (expr)? ';'
  std::unique_ptr<ASTNode> init = nullptr;
  if (peek().type == TOK_KW_INT) {
    init = parse_var_decl();
  } else if (match(TOK_SEMICOLON)) {
    init = nullptr;
  } else {
    init = parse_expr();
    expect(TOK_SEMICOLON, "Expected ';' after for init");
  }

  // 2. Cond: (expr)? ';'
  std::unique_ptr<ASTNode> cond = nullptr;
  if (match(TOK_SEMICOLON)) {
    cond = nullptr;
  } else {
    cond = parse_expr();
    expect(TOK_SEMICOLON, "Expected ';' after for cond");
  }

  // 3. Step: (expr)? ')'
  std::unique_ptr<ASTNode> step = nullptr;
  if (match(TOK_RPAREN)) {
    step = nullptr;
  } else {
    step = parse_expr();
    expect(TOK_RPAREN, "Expected ')' after for step");
  }

  // 4. Body
  auto body = parse_statement();
  return std::make_unique<ForNode>(t.line, std::move(init), std::move(cond),
                                  std::move(step), std::move(body));
}

std::unique_ptr<WhileNode> Parser::parse_while() {
  Token t = expect(TOK_KW_WHILE, "Expected 'while'");
  expect(TOK_LPAREN, "Expected '(' after 'while'");
  auto cond = parse_expr();
  expect(TOK_RPAREN, "Expected ')' after while condition");
  auto body = parse_statement();
  return std::make_unique<WhileNode>(t.line, std::move(cond), std::move(body));
}

std::unique_ptr<DoWhileNode> Parser::parse_do_while() {
  Token t = expect(TOK_KW_DO, "Expected 'do'");
  auto body = parse_statement();
  expect(TOK_KW_WHILE, "Expected 'while' after do body");
  expect(TOK_LPAREN, "Expected '(' after 'while'");
  auto cond = parse_expr();
  expect(TOK_RPAREN, "Expected ')' after do-while condition");
  expect(TOK_SEMICOLON, "Expected ';' after do-while loop");
  return std::make_unique<DoWhileNode>(t.line, std::move(body), std::move(cond));
}

std::unique_ptr<SwitchNode> Parser::parse_switch() {
  Token t = expect(TOK_KW_SWITCH, "Expected 'switch'");
  expect(TOK_LPAREN, "Expected '(' after 'switch'");
  auto cond = parse_expr();
  expect(TOK_RPAREN, "Expected ')' after switch condition");
  expect(TOK_LBRACE, "Expected '{' before switch body");

  std::vector<std::unique_ptr<CaseNode>> cases;

  while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
    if (peek().type == TOK_KW_CASE) {
      Token case_tok = expect(TOK_KW_CASE, "Expected 'case'");
      auto val = parse_expr();
      expect(TOK_COLON, "Expected ':' after case value");

      std::vector<std::unique_ptr<ASTNode>> stmts;
      while (peek().type != TOK_KW_CASE &&
             peek().type != TOK_KW_DEFAULT &&
             peek().type != TOK_RBRACE &&
             peek().type != TOK_EOF) {
        stmts.push_back(parse_statement());
      }
      cases.push_back(std::make_unique<CaseNode>(case_tok.line, std::move(val),
                                                 std::move(stmts), false));
    } else if (peek().type == TOK_KW_DEFAULT) {
      Token def_tok = expect(TOK_KW_DEFAULT, "Expected 'default'");
      expect(TOK_COLON, "Expected ':' after 'default'");

      std::vector<std::unique_ptr<ASTNode>> stmts;
      while (peek().type != TOK_KW_CASE &&
             peek().type != TOK_KW_DEFAULT &&
             peek().type != TOK_RBRACE &&
             peek().type != TOK_EOF) {
        stmts.push_back(parse_statement());
      }
      cases.push_back(std::make_unique<CaseNode>(def_tok.line, nullptr,
                                                 std::move(stmts), true));
    } else {
      parse_error(peek(), "Expected 'case' or 'default' inside switch body");
    }
  }

  expect(TOK_RBRACE, "Expected '}' after switch body");
  return std::make_unique<SwitchNode>(t.line, std::move(cond), std::move(cases));
}

std::unique_ptr<BreakNode> Parser::parse_break() {
  Token t = expect(TOK_KW_BREAK, "Expected 'break'");
  expect(TOK_SEMICOLON, "Expected ';' after 'break'");
  return std::make_unique<BreakNode>(t.line);
}

std::unique_ptr<ReturnNode> Parser::parse_return() {
  Token t = expect(TOK_KW_RETURN, "Expected 'return'");
  auto expr = parse_expr();
  expect(TOK_SEMICOLON, "Expected ';' after return expression");
  return std::make_unique<ReturnNode>(t.line, std::move(expr));
}

std::unique_ptr<ASTNode> Parser::parse_statement() {
  Token t = peek();

  if (t.type == TOK_KW_INT) {
    return parse_var_decl();
  }
  if (t.type == TOK_KW_FOR) {
    return parse_for();
  }
  if (t.type == TOK_KW_WHILE) {
    return parse_while();
  }
  if (t.type == TOK_KW_DO) {
    return parse_do_while();
  }
  if (t.type == TOK_KW_SWITCH) {
    return parse_switch();
  }
  if (t.type == TOK_KW_BREAK) {
    return parse_break();
  }
  if (t.type == TOK_KW_IF) {
    return parse_if();
  }
  if (t.type == TOK_KW_RETURN) {
    return parse_return();
  }
  if (t.type == TOK_LBRACE) {
    return parse_compound_statement();
  }

  auto expr = parse_expr();
  expect(TOK_SEMICOLON, "Expected ';' after expression");
  return expr;
}

std::unique_ptr<ProgramNode> Parser::parse_program() {
  auto prog = std::make_unique<ProgramNode>(1);
  while (peek().type != TOK_EOF) {
    prog->append(parse_statement());
  }
  return prog;
}
