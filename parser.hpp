#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include "lexer.hpp"
#include <memory>
#include <stdexcept>
#include <string>

class ParseException : public std::runtime_error {
public:
  int line;
  int col;
  ParseException(const std::string &msg, int line, int col)
      : std::runtime_error(msg), line(line), col(col) {}
};

class Parser {
public:
  explicit Parser(Lexer *lexer);

  // Top-level & Statements (implemented in parser_stmt.cpp)
  std::unique_ptr<ProgramNode> parse_program();
  std::unique_ptr<ASTNode> parse_statement();
  std::unique_ptr<IfNode> parse_if();
  std::unique_ptr<ForNode> parse_for();
  std::unique_ptr<WhileNode> parse_while();
  std::unique_ptr<DoWhileNode> parse_do_while();
  std::unique_ptr<SwitchNode> parse_switch();
  std::unique_ptr<BreakNode> parse_break();
  std::unique_ptr<ContinueNode> parse_continue();
  std::unique_ptr<GotoNode> parse_goto();
  std::unique_ptr<ReturnNode> parse_return();
  std::unique_ptr<CompoundNode> parse_compound_statement();

  // Declarations (implemented in parser_decl.cpp)
  std::unique_ptr<VarDeclNode> parse_var_decl();

  // Expressions (implemented in parser_expr.cpp)
  std::unique_ptr<ASTNode> parse_expr();

  // Core parsing helpers (implemented in parser_core.cpp)
  [[noreturn]] void parse_error(const Token &tok, const std::string &msg);
  Token expect(TokenType type, const std::string &err_msg);
  bool match(TokenType type);
  Token peek(size_t k = 1);

  // Utility combinators
  template <typename Fn>
  auto parse_parenthesized(Fn &&fn,
                           const std::string &lparen_err = "Expected '('",
                           const std::string &rparen_err = "Expected ')'")
      -> decltype(fn()) {
    expect(TOK_LPAREN, lparen_err);
    auto res = fn();
    expect(TOK_RPAREN, rparen_err);
    return res;
  }

private:
  Lexer *lexer_;

  // Expression parsing helpers (parser_expr.cpp)
  std::unique_ptr<ASTNode> parse_assignment();
  std::unique_ptr<ASTNode> parse_expr_precedence(int min_prec);
  std::unique_ptr<ASTNode> parse_unary();
  std::unique_ptr<ASTNode> parse_primary();
};

#endif // PARSER_HPP
