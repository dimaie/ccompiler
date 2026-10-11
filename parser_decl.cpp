#include "parser.hpp"
#include <utility>

bool Parser::is_type_specifier(TokenType type) const {
  return type == TOK_KW_INT || type == TOK_KW_SHORT || type == TOK_KW_LONG ||
         type == TOK_KW_CHAR || type == TOK_KW_VOID || type == TOK_KW_STRUCT ||
         type == TOK_KW_UNION || type == TOK_KW_CONST || type == TOK_KW_VOLATILE;
}

DataType Parser::parse_type_specifier() {
  while (match(TOK_KW_CONST) || match(TOK_KW_VOLATILE)) {}
  Token t = peek();
  DataType result = DataType::Int;
  if (match(TOK_KW_VOID)) {
    result = DataType::Void;
  } else if (match(TOK_KW_CHAR)) {
    result = DataType::Char;
  } else if (match(TOK_KW_SHORT)) {
    match(TOK_KW_INT); // optional 'int' after 'short'
    result = DataType::Short;
  } else if (match(TOK_KW_LONG)) {
    if (match(TOK_KW_LONG)) {
      match(TOK_KW_INT); // optional 'int' after 'long long'
      result = DataType::LongLong;
    } else {
      match(TOK_KW_INT); // optional 'int' after 'long'
      result = DataType::Long;
    }
  } else if (match(TOK_KW_INT)) {
    if (match(TOK_KW_SHORT)) {
      result = DataType::Short;
    } else if (match(TOK_KW_LONG)) {
      if (match(TOK_KW_LONG)) {
        result = DataType::LongLong;
      } else {
        result = DataType::Long;
      }
    } else {
      result = DataType::Int;
    }
  } else {
    parse_error(t, "Expected type specifier ('int', 'short', 'long', 'char', or 'void')");
  }
  while (match(TOK_KW_CONST) || match(TOK_KW_VOLATILE)) {}
  return result;
}

static std::vector<DataType> parse_func_ptr_param_list(Parser *parser, Lexer *lexer) {
  std::vector<DataType> param_types;
  if (parser->peek().type != TOK_RPAREN) {
    if (parser->peek().type == TOK_KW_VOID && parser->peek(2).type == TOK_RPAREN) {
      lexer->next(); // consume 'void'
    } else {
      while (true) {
        DataType pt = parser->parse_type_specifier();
        while (parser->match(TOK_STAR)) {
          // ignore star on prototype param
        }
        if (parser->peek().type == TOK_IDENTIFIER) {
          lexer->next(); // optional param name in prototype
        }
        param_types.push_back(pt);
        if (parser->match(TOK_COMMA)) {
          continue;
        }
        break;
      }
    }
  }
  return param_types;
}

std::unique_ptr<VarDeclNode> Parser::parse_func_ptr_decl(DataType return_type) {
  expect(TOK_LPAREN, "Expected '(' in function pointer declaration");
  int ptr_level = 0;
  while (match(TOK_STAR)) {
    ++ptr_level;
  }
  if (ptr_level == 0) {
    ptr_level = 1;
  }
  Token id_tok = expect(TOK_IDENTIFIER, "Expected function pointer name");
  expect(TOK_RPAREN, "Expected ')' after function pointer name");
  expect(TOK_LPAREN, "Expected '(' for function pointer parameter list");
  std::vector<DataType> param_types = parse_func_ptr_param_list(this, lexer_);
  expect(TOK_RPAREN, "Expected ')' after parameters");
  std::unique_ptr<ASTNode> init = nullptr;
  if (match(TOK_ASSIGN)) {
    init = parse_expr();
  }
  expect(TOK_SEMICOLON, "Expected ';' after function pointer declaration");
  auto node = std::make_unique<VarDeclNode>(id_tok.line, return_type, id_tok.text, std::move(init));
  node->is_func_ptr = true;
  node->pointer_level = ptr_level;
  node->func_ptr_ret_type = return_type;
  node->func_ptr_params = std::move(param_types);
  return node;
}

std::unique_ptr<ASTNode> Parser::parse_declaration() {
  if (peek().type == TOK_KW_STRUCT || peek().type == TOK_KW_UNION) {
    return parse_struct_or_union_decl();
  }
  DataType type = parse_type_specifier();
  if (peek().type == TOK_LPAREN && peek(2).type == TOK_STAR) {
    return parse_func_ptr_decl(type);
  }
  int pointer_level = 0;
  while (match(TOK_STAR)) {
    ++pointer_level;
  }
  Token id_tok = expect(TOK_IDENTIFIER, "Expected identifier after type specifier");
  if (peek().type == TOK_LPAREN) {
    return parse_function_def(type, id_tok, pointer_level);
  }
  if (type == DataType::Void && pointer_level == 0) {
    parse_error(id_tok, "Variable cannot have type 'void'");
  }
  return parse_var_decl_after_id(type, id_tok, pointer_level);
}

std::unique_ptr<ASTNode> Parser::parse_struct_or_union_decl() {
  Token kw_tok = peek();
  bool is_union = match(TOK_KW_UNION);
  if (!is_union) {
    expect(TOK_KW_STRUCT, "Expected 'struct' or 'union'");
  }

  std::string tag;
  if (peek().type == TOK_IDENTIFIER) {
    tag = lexer_->next().text;
  }

  if (match(TOK_LBRACE)) {
    std::vector<StructMember> members;
    while (peek().type != TOK_RBRACE && peek().type != TOK_EOF) {
      StructMember m;
      if (peek().type == TOK_KW_STRUCT || peek().type == TOK_KW_UNION) {
        m.is_union = match(TOK_KW_UNION);
        if (!m.is_union) expect(TOK_KW_STRUCT, "Expected 'struct'");
        m.type = m.is_union ? DataType::Union : DataType::Struct;
        if (peek().type == TOK_IDENTIFIER) {
          m.struct_name = lexer_->next().text;
        }
      } else {
        m.type = parse_type_specifier();
      }
      while (match(TOK_STAR)) {
        m.pointer_level++;
      }
      Token mem_id = expect(TOK_IDENTIFIER, "Expected member name");
      m.name = mem_id.text;
      if (match(TOK_LBRACKET)) {
        m.is_array = true;
        Token sz_tok = expect(TOK_INT_LIT, "Expected array size");
        m.array_size = static_cast<int>(sz_tok.value.int_val);
        expect(TOK_RBRACKET, "Expected ']' after array size");
      }
      expect(TOK_SEMICOLON, "Expected ';' after member declaration");
      members.push_back(std::move(m));
    }
    expect(TOK_RBRACE, "Expected '}' after struct/union body");

    if (match(TOK_SEMICOLON)) {
      return std::make_unique<StructDefNode>(kw_tok.line, is_union, tag, std::move(members));
    }

    int ptr_level = 0;
    while (match(TOK_STAR)) {
      ptr_level++;
    }
    Token id_tok = expect(TOK_IDENTIFIER, "Expected variable name after struct/union definition");
    auto var_decl = parse_var_decl_after_id(is_union ? DataType::Union : DataType::Struct, id_tok, ptr_level);
    var_decl->struct_name = tag;
    return std::make_unique<StructDefNode>(kw_tok.line, is_union, tag, std::move(members), std::move(var_decl));
  }

  if (tag.empty()) {
    parse_error(peek(), "Expected struct or union tag name");
  }

  int ptr_level = 0;
  while (match(TOK_STAR)) {
    ptr_level++;
  }
  Token id_tok = expect(TOK_IDENTIFIER, "Expected variable or function name after struct/union type");
  if (peek().type == TOK_LPAREN) {
    auto func = parse_function_def(is_union ? DataType::Union : DataType::Struct, id_tok, ptr_level);
    func->return_struct_name = tag;
    return func;
  }

  auto var_decl = parse_var_decl_after_id(is_union ? DataType::Union : DataType::Struct, id_tok, ptr_level);
  var_decl->struct_name = tag;
  return var_decl;
}

std::unique_ptr<FuncDefNode>
Parser::parse_function_def(DataType return_type, const Token &id_tok, int return_pointer_level) {
  expect(TOK_LPAREN, "Expected '(' after function name");
  std::vector<Param> params;
  if (peek().type != TOK_RPAREN) {
    if (peek().type == TOK_KW_VOID && peek(2).type == TOK_RPAREN) {
      lexer_->next(); // consume 'void'
    } else {
      while (true) {
        DataType param_type = DataType::Int;
        std::string param_struct_name;
        if (peek().type == TOK_KW_STRUCT || peek().type == TOK_KW_UNION) {
          bool is_u = match(TOK_KW_UNION);
          if (!is_u) expect(TOK_KW_STRUCT, "Expected 'struct'");
          param_type = is_u ? DataType::Union : DataType::Struct;
          Token tag_tok = expect(TOK_IDENTIFIER, "Expected struct/union tag name");
          param_struct_name = tag_tok.text;
        } else {
          param_type = parse_type_specifier();
        }
        if (peek().type == TOK_LPAREN && peek(2).type == TOK_STAR) {
          expect(TOK_LPAREN, "Expected '(' in function pointer parameter");
          int ptr_level = 0;
          while (match(TOK_STAR)) {
            ++ptr_level;
          }
          if (ptr_level == 0) ptr_level = 1;
          Token param_name = expect(TOK_IDENTIFIER, "Expected parameter name");
          expect(TOK_RPAREN, "Expected ')' after parameter name");
          expect(TOK_LPAREN, "Expected '(' for parameter list");
          std::vector<DataType> p_param_types = parse_func_ptr_param_list(this, lexer_);
          expect(TOK_RPAREN, "Expected ')' after parameter list");
          Param p;
          p.type = param_type;
          p.pointer_level = ptr_level;
          p.name = param_name.text;
          p.line = param_name.line;
          p.is_func_ptr = true;
          p.func_ptr_ret_type = param_type;
          p.func_ptr_params = std::move(p_param_types);
          p.struct_name = param_struct_name;
          params.push_back(std::move(p));
        } else {
          int param_ptr_level = 0;
          while (match(TOK_STAR)) {
            ++param_ptr_level;
          }
          Token param_name =
              expect(TOK_IDENTIFIER, "Expected parameter name");
          if (param_type == DataType::Void && param_ptr_level == 0) {
            parse_error(param_name, "Parameter cannot have type 'void'");
          }
          Param p;
          p.type = param_type;
          p.pointer_level = param_ptr_level;
          p.name = param_name.text;
          p.line = param_name.line;
          p.struct_name = param_struct_name;
          params.push_back(std::move(p));
        }

        if (match(TOK_COMMA)) {
          continue;
        }
        break;
      }
    }
  }
  expect(TOK_RPAREN, "Expected ')' after parameters");
  if (match(TOK_SEMICOLON)) {
    return std::make_unique<FuncDefNode>(id_tok.line, return_type, id_tok.text,
                                         std::move(params), nullptr,
                                         return_pointer_level);
  }
  auto body = parse_compound_statement();
  return std::make_unique<FuncDefNode>(id_tok.line, return_type, id_tok.text,
                                       std::move(params), std::move(body),
                                       return_pointer_level);
}

std::unique_ptr<VarDeclNode>
Parser::parse_var_decl_after_id(DataType type, const Token &id_tok, int pointer_level) {
  bool is_array = false;
  int array_size = 0;

  if (match(TOK_LBRACKET)) {
    is_array = true;
    if (peek().type == TOK_INT_LIT) {
      Token sz_tok = expect(TOK_INT_LIT, "Expected integer literal for array size");
      array_size = static_cast<int>(sz_tok.value.int_val);
      if (array_size <= 0) {
        parse_error(sz_tok, "Array size must be positive");
      }
    } else if (peek().type == TOK_RBRACKET) {
      array_size = -1; // to be inferred from initializer list
    } else {
      parse_error(peek(), "Expected integer literal or ']' in array declaration");
    }
    expect(TOK_RBRACKET, "Expected ']' after array size");
  }

  std::unique_ptr<ASTNode> init = nullptr;
  if (match(TOK_ASSIGN)) {
    if (peek().type == TOK_LBRACE) {
      if (!is_array && type != DataType::Struct && type != DataType::Union) {
        parse_error(peek(), "Scalar variable cannot be initialized with initializer list");
      }
      Token lbrace_tok = expect(TOK_LBRACE, "Expected '{'");
      std::vector<std::unique_ptr<ASTNode>> elements;
      if (peek().type != TOK_RBRACE) {
        while (true) {
          elements.push_back(parse_expr());
          if (match(TOK_COMMA)) {
            if (peek().type == TOK_RBRACE) {
              break;
            }
            continue;
          }
          break;
        }
      }
      expect(TOK_RBRACE, "Expected '}' after initializer list");
      if (is_array) {
        if (array_size == -1) {
          array_size = static_cast<int>(elements.size());
        } else if (static_cast<int>(elements.size()) > array_size) {
          parse_error(lbrace_tok, "Too many initializers for array");
        }
      }
      init = std::make_unique<InitListNode>(lbrace_tok.line, std::move(elements));
    } else {
      if (is_array) {
        parse_error(peek(), "Array initializer must be an initializer list '{...}'");
      }
      init = parse_expr();
    }
  }

  if (is_array && array_size <= 0) {
    parse_error(id_tok, "Array size must be positive");
  }

  expect(TOK_SEMICOLON, "Expected ';' after variable declaration");
  if (is_array) {
    auto node = std::make_unique<VarDeclNode>(id_tok.line, type, id_tok.text,
                                              array_size, std::move(init));
    node->pointer_level = pointer_level;
    return node;
  }
  return std::make_unique<VarDeclNode>(id_tok.line, type, pointer_level, id_tok.text,
                                       std::move(init));
}

std::unique_ptr<VarDeclNode> Parser::parse_var_decl() {
  if (peek().type == TOK_KW_STRUCT || peek().type == TOK_KW_UNION) {
    auto node = parse_struct_or_union_decl();
    if (node->getType() == ASTNodeType::VarDecl) {
      return std::unique_ptr<VarDeclNode>(static_cast<VarDeclNode *>(node.release()));
    }
    if (node->getType() == ASTNodeType::StructDef) {
      auto *sd = static_cast<StructDefNode *>(node.get());
      if (sd->var_decl) {
        return std::move(sd->var_decl);
      }
    }
    parse_error(peek(), "Expected variable declaration");
  }
  DataType type = parse_type_specifier();
  if (peek().type == TOK_LPAREN && peek(2).type == TOK_STAR) {
    return parse_func_ptr_decl(type);
  }
  int pointer_level = 0;
  while (match(TOK_STAR)) {
    ++pointer_level;
  }
  Token id_tok = expect(TOK_IDENTIFIER, "Expected identifier after type specifier");
  if (type == DataType::Void && pointer_level == 0) {
    parse_error(id_tok, "Variable cannot have type 'void'");
  }
  return parse_var_decl_after_id(type, id_tok, pointer_level);
}
