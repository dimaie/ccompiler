#include "parser.hpp"
#include <utility>

bool Parser::is_type_specifier(TokenType type) const {
  return type == TOK_KW_INT || type == TOK_KW_SHORT || type == TOK_KW_LONG;
}

DataType Parser::parse_type_specifier() {
  Token t = peek();
  if (match(TOK_KW_SHORT)) {
    match(TOK_KW_INT); // optional 'int' after 'short'
    return DataType::Short;
  }
  if (match(TOK_KW_LONG)) {
    if (match(TOK_KW_LONG)) {
      match(TOK_KW_INT); // optional 'int' after 'long long'
      return DataType::LongLong;
    }
    match(TOK_KW_INT); // optional 'int' after 'long'
    return DataType::Long;
  }
  if (match(TOK_KW_INT)) {
    if (match(TOK_KW_SHORT)) {
      return DataType::Short;
    }
    if (match(TOK_KW_LONG)) {
      if (match(TOK_KW_LONG)) {
        return DataType::LongLong;
      }
      return DataType::Long;
    }
    return DataType::Int;
  }
  parse_error(t, "Expected type specifier ('int', 'short', or 'long')");
}

std::unique_ptr<VarDeclNode> Parser::parse_var_decl() {
  DataType type = parse_type_specifier();
  Token id_tok = expect(TOK_IDENTIFIER, "Expected identifier after type specifier");
  std::unique_ptr<ASTNode> init = nullptr;

  if (match(TOK_ASSIGN)) {
    init = parse_expr();
  }
  expect(TOK_SEMICOLON, "Expected ';' after variable declaration");
  return std::make_unique<VarDeclNode>(id_tok.line, type, id_tok.text,
                                       std::move(init));
}
