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
  if (is_type_specifier(peek().type)) {
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
        auto s = parse_statement();
        if (s) {
          stmts.push_back(std::move(s));
        }
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
        auto s = parse_statement();
        if (s) {
          stmts.push_back(std::move(s));
        }
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

std::unique_ptr<ContinueNode> Parser::parse_continue() {
  Token t = expect(TOK_KW_CONTINUE, "Expected 'continue'");
  expect(TOK_SEMICOLON, "Expected ';' after 'continue'");
  return std::make_unique<ContinueNode>(t.line);
}

std::unique_ptr<GotoNode> Parser::parse_goto() {
  Token t = expect(TOK_KW_GOTO, "Expected 'goto'");
  Token label_tok = expect(TOK_IDENTIFIER, "Expected label identifier after 'goto'");
  expect(TOK_SEMICOLON, "Expected ';' after goto statement");
  return std::make_unique<GotoNode>(t.line, label_tok.text);
}

std::unique_ptr<ReturnNode> Parser::parse_return() {
  Token t = expect(TOK_KW_RETURN, "Expected 'return'");
  std::unique_ptr<ASTNode> expr = nullptr;
  if (peek().type != TOK_SEMICOLON) {
    expr = parse_expr();
  }
  expect(TOK_SEMICOLON, "Expected ';' after return");
  return std::make_unique<ReturnNode>(t.line, std::move(expr));
}

std::unique_ptr<ASTNode> Parser::parse_statement() {
  Token t = peek();
  std::unique_ptr<ASTNode> res = nullptr;

  if (t.type == TOK_KW_STATIC || t.type == TOK_KW_EXTERN ||
      is_type_specifier(t.type) || is_calling_convention(t.type)) {
    res = parse_declaration();
  } else if (t.type == TOK_KW_FOR) {
    res = parse_for();
  } else if (t.type == TOK_KW_WHILE) {
    res = parse_while();
  } else if (t.type == TOK_KW_DO) {
    res = parse_do_while();
  } else if (t.type == TOK_KW_SWITCH) {
    res = parse_switch();
  } else if (t.type == TOK_KW_BREAK) {
    res = parse_break();
  } else if (t.type == TOK_KW_CONTINUE) {
    res = parse_continue();
  } else if (t.type == TOK_KW_GOTO) {
    res = parse_goto();
  } else if (t.type == TOK_IDENTIFIER && peek(2).type == TOK_COLON) {
    Token label_tok = expect(TOK_IDENTIFIER, "Expected label identifier");
    expect(TOK_COLON, "Expected ':' after label");
    res = std::make_unique<LabelNode>(label_tok.line, label_tok.text);
  } else if (t.type == TOK_SEMICOLON) {
    expect(TOK_SEMICOLON, "Expected ';'");
    return nullptr;
  } else if (t.type == TOK_KW_ASM) {
    res = parse_asm();
  } else if (t.type == TOK_KW_IF) {
    res = parse_if();
  } else if (t.type == TOK_KW_RETURN) {
    res = parse_return();
  } else if (t.type == TOK_LBRACE) {
    res = parse_compound_statement();
  } else {
    res = parse_expr();
    expect(TOK_SEMICOLON, "Expected ';' after expression");
  }

  if (res) {
    res->file_id = t.file_id;
  }
  return res;
}

std::unique_ptr<AsmStmtNode> Parser::parse_asm() {
  Token asm_tok = expect(TOK_KW_ASM, "Expected 'asm'");
  bool is_volatile = false;
  if (match(TOK_KW_VOLATILE)) {
    is_volatile = true;
  }

  expect(TOK_LPAREN, "Expected '(' after asm");
  Token s_tok = expect(TOK_STRING_LIT, "Expected string literal in asm statement");
  std::string asm_code = s_tok.text;

  // Support string concatenation: asm("line 1\n" "line 2\n")
  while (peek().type == TOK_STRING_LIT) {
    Token next_str = expect(TOK_STRING_LIT, "Expected string literal");
    asm_code += "\n";
    asm_code += next_str.text;
  }

  // Support optional extended asm operands (: output : input : clobbers) by skipping to ')'
  int paren_depth = 0;
  while (peek().type != TOK_EOF) {
    if (peek().type == TOK_LPAREN) {
      paren_depth++;
      lexer_->next();
    } else if (peek().type == TOK_RPAREN) {
      if (paren_depth > 0) {
        paren_depth--;
        lexer_->next();
      } else {
        break; // Reached matching closing ')'
      }
    } else {
      lexer_->next();
    }
  }

  expect(TOK_RPAREN, "Expected ')' after asm code");
  expect(TOK_SEMICOLON, "Expected ';' after asm statement");
  auto asm_node = std::make_unique<AsmStmtNode>(asm_tok.line, asm_code, is_volatile);
  asm_node->file_id = asm_tok.file_id;
  return asm_node;
}

std::unique_ptr<ProgramNode> Parser::parse_program() {
  auto prog = std::make_unique<ProgramNode>(1);
  while (peek().type != TOK_EOF) {
    prog->append(parse_statement());
  }
  if (lexer_) {
    prog->file_table = lexer_->getFileTable();
  }
  return prog;
}
