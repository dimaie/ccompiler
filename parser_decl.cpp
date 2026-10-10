#include "parser.hpp"
#include <utility>

bool Parser::is_type_specifier(TokenType type) const {
  return type == TOK_KW_INT || type == TOK_KW_SHORT || type == TOK_KW_LONG ||
         type == TOK_KW_VOID;
}

DataType Parser::parse_type_specifier() {
  Token t = peek();
  if (match(TOK_KW_VOID)) {
    return DataType::Void;
  }
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
  parse_error(t, "Expected type specifier ('int', 'short', 'long', or 'void')");
}

std::unique_ptr<ASTNode> Parser::parse_declaration() {
  DataType type = parse_type_specifier();
  Token id_tok = expect(TOK_IDENTIFIER, "Expected identifier after type specifier");
  if (peek().type == TOK_LPAREN) {
    return parse_function_def(type, id_tok);
  }
  if (type == DataType::Void) {
    parse_error(id_tok, "Variable cannot have type 'void'");
  }
  return parse_var_decl_after_id(type, id_tok);
}

std::unique_ptr<FuncDefNode>
Parser::parse_function_def(DataType return_type, const Token &id_tok) {
  expect(TOK_LPAREN, "Expected '(' after function name");
  std::vector<Param> params;
  if (peek().type != TOK_RPAREN) {
    if (peek().type == TOK_KW_VOID && peek(2).type == TOK_RPAREN) {
      lexer_->next(); // consume 'void'
    } else {
      while (true) {
        DataType param_type = parse_type_specifier();
        Token param_name =
            expect(TOK_IDENTIFIER, "Expected parameter name");
        if (param_type == DataType::Void) {
          parse_error(param_name, "Parameter cannot have type 'void'");
        }
        params.push_back(Param{param_type, param_name.text, param_name.line});
        if (match(TOK_COMMA)) {
          continue;
        }
        break;
      }
    }
  }
  expect(TOK_RPAREN, "Expected ')' after parameters");
  auto body = parse_compound_statement();
  return std::make_unique<FuncDefNode>(id_tok.line, return_type, id_tok.text,
                                       std::move(params), std::move(body));
}

std::unique_ptr<VarDeclNode>
Parser::parse_var_decl_after_id(DataType type, const Token &id_tok) {
  std::unique_ptr<ASTNode> init = nullptr;
  if (match(TOK_ASSIGN)) {
    init = parse_expr();
  }
  expect(TOK_SEMICOLON, "Expected ';' after variable declaration");
  return std::make_unique<VarDeclNode>(id_tok.line, type, id_tok.text,
                                       std::move(init));
}

std::unique_ptr<VarDeclNode> Parser::parse_var_decl() {
  DataType type = parse_type_specifier();
  Token id_tok = expect(TOK_IDENTIFIER, "Expected identifier after type specifier");
  if (type == DataType::Void) {
    parse_error(id_tok, "Variable cannot have type 'void'");
  }
  return parse_var_decl_after_id(type, id_tok);
}
