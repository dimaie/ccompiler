#include "parser.hpp"

Parser::Parser(Lexer *lexer) : lexer_(lexer) {
  typedefs_["va_list"] = {DataType::Char, 1};
  register_scopes_.emplace_back();
}

void Parser::push_scope() {
  register_scopes_.emplace_back();
  if (!pending_register_params_.empty()) {
    for (const auto &p : pending_register_params_) {
      register_scopes_.back().insert(p);
    }
    pending_register_params_.clear();
  }
}

void Parser::pop_scope() {
  if (!register_scopes_.empty()) {
    register_scopes_.pop_back();
  }
}

void Parser::register_var_name(const std::string &name) {
  if (!register_scopes_.empty()) {
    register_scopes_.back().insert(name);
  }
}

bool Parser::is_register_var(const std::string &name) const {
  for (auto it = register_scopes_.rbegin(); it != register_scopes_.rend(); ++it) {
    if (it->find(name) != it->end()) {
      return true;
    }
  }
  return false;
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
