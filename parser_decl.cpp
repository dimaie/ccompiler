#include "parser.hpp"
#include <utility>

std::unique_ptr<VarDeclNode> Parser::parse_var_decl() {
  expect(TOK_KW_INT, "Expected type 'int'");
  Token id_tok = expect(TOK_IDENTIFIER, "Expected identifier after 'int'");
  std::unique_ptr<ASTNode> init = nullptr;

  if (match(TOK_ASSIGN)) {
    init = parse_expr();
  }
  expect(TOK_SEMICOLON, "Expected ';' after variable declaration");
  return std::make_unique<VarDeclNode>(id_tok.line, id_tok.text,
                                       std::move(init));
}
