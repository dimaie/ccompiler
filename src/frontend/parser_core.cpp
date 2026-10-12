#include "parser.hpp"

Parser::Parser(Lexer *lexer) : lexer_(lexer) {
  typedefs_["va_list"] = {DataType::Char, 1};
}

void Parser::parse_error(const Token &tok, const std::string &msg) {
  std::string full_msg = "Syntax Error [Line " + std::to_string(tok.line) +
                         ", Col " + std::to_string(tok.col) + "] near '" +
                         tok.text + "': " + msg;
  throw ParseException(full_msg, tok.line, tok.col);
}

Token Parser::expect(TokenType type, const std::string &err_msg) {
  Token t = lexer_->next();
  if (t.type != type) {
    parse_error(t, err_msg);
  }
  return t;
}

bool Parser::match(TokenType type) {
  if (lexer_->peek().type == type) {
    lexer_->next();
    return true;
  }
  return false;
}

Token Parser::peek(size_t k) {
  return lexer_->peek(k);
}
