#include "parser.hpp"
#include <unordered_map>
#include <utility>

namespace {

struct BinaryOpInfo {
  int precedence;
  bool right_associative;
};

// Operator Precedence Table (higher value = higher precedence)
const std::unordered_map<TokenType, BinaryOpInfo> BINARY_OPS = {
    {TOK_LOGIC_OR,   {2,  false}}, // ||
    {TOK_LOGIC_AND,  {3,  false}}, // &&
    {TOK_PIPE,       {4,  false}}, // |
    {TOK_CARET,      {5,  false}}, // ^
    {TOK_AMP,        {6,  false}}, // &
    {TOK_EQUAL,      {7,  false}}, // ==
    {TOK_NOT_EQUAL,  {7,  false}}, // !=
    {TOK_LESS,       {8,  false}}, // <
    {TOK_LESS_EQ,    {8,  false}}, // <=
    {TOK_GREATER,    {8,  false}}, // >
    {TOK_GREATER_EQ, {8,  false}}, // >=
    {TOK_LSHIFT,     {9,  false}}, // <<
    {TOK_RSHIFT,     {9,  false}}, // >>
    {TOK_PLUS,       {10, false}}, // +
    {TOK_MINUS,      {10, false}}, // -
    {TOK_STAR,       {11, false}}, // *
    {TOK_SLASH,      {11, false}}, // /
    {TOK_PERCENT,    {11, false}}, // %
};

std::unique_ptr<ASTNode> clone_ast(const ASTNode *node) {
  if (!node) return nullptr;
  switch (node->getType()) {
  case ASTNodeType::Identifier: {
    auto *id = static_cast<const IdentifierNode *>(node);
    return std::make_unique<IdentifierNode>(id->line, id->name);
  }
  case ASTNodeType::IntLit: {
    auto *lit = static_cast<const IntLitNode *>(node);
    return std::make_unique<IntLitNode>(lit->line, lit->val);
  }
  case ASTNodeType::StringLit: {
    auto *lit = static_cast<const StringLitNode *>(node);
    return std::make_unique<StringLitNode>(lit->line, lit->value);
  }
  case ASTNodeType::ArrayAccess: {
    auto *acc = static_cast<const ArrayAccessNode *>(node);
    return std::make_unique<ArrayAccessNode>(acc->line, clone_ast(acc->array.get()), clone_ast(acc->index.get()));
  }
  case ASTNodeType::MemberAccess: {
    auto *mem = static_cast<const MemberAccessNode *>(node);
    return std::make_unique<MemberAccessNode>(mem->line, clone_ast(mem->object.get()), mem->member, mem->is_arrow);
  }
  case ASTNodeType::UnaryOp: {
    auto *un = static_cast<const UnaryOpNode *>(node);
    return std::make_unique<UnaryOpNode>(un->line, un->op, clone_ast(un->expr.get()), un->is_postfix);
  }
  case ASTNodeType::BinaryOp: {
    auto *bin = static_cast<const BinaryOpNode *>(node);
    return std::make_unique<BinaryOpNode>(bin->line, bin->op, clone_ast(bin->left.get()), clone_ast(bin->right.get()));
  }
  case ASTNodeType::TernaryOp: {
    auto *ter = static_cast<const TernaryOpNode *>(node);
    return std::make_unique<TernaryOpNode>(ter->line, clone_ast(ter->cond.get()), clone_ast(ter->then_expr.get()), clone_ast(ter->else_expr.get()));
  }
  case ASTNodeType::Assign: {
    auto *as = static_cast<const AssignNode *>(node);
    return std::make_unique<AssignNode>(as->line, clone_ast(as->left.get()), clone_ast(as->right.get()));
  }
  case ASTNodeType::Call: {
    auto *call = static_cast<const CallNode *>(node);
    std::vector<std::unique_ptr<ASTNode>> args;
    for (const auto &arg : call->args) {
      args.push_back(clone_ast(arg.get()));
    }
    if (call->callee_expr) {
      return std::make_unique<CallNode>(call->line, clone_ast(call->callee_expr.get()), std::move(args));
    } else {
      return std::make_unique<CallNode>(call->line, call->callee, std::move(args));
    }
  }
  default:
    return nullptr;
  }
}

} // namespace

std::unique_ptr<ASTNode> Parser::parse_primary() {
  Token t = peek();
  std::unique_ptr<ASTNode> expr = nullptr;

  if (t.type == TOK_INT_LIT) {
    lexer_->next();
    expr = std::make_unique<IntLitNode>(t.line, t.value.int_val);
  } else if (t.type == TOK_CHAR_LIT) {
    lexer_->next();
    expr = std::make_unique<IntLitNode>(t.line, t.value.int_val);
  } else if (t.type == TOK_STRING_LIT) {
    lexer_->next();
    std::string s = t.text;
    while (peek().type == TOK_STRING_LIT) {
      s += lexer_->next().text;
    }
    expr = std::make_unique<StringLitNode>(t.line, std::move(s));
  } else if (t.type == TOK_IDENTIFIER) {
    lexer_->next();
    std::string id_name = t.text;
    if (peek().type == TOK_LPAREN) {
      if (id_name == "va_start" || id_name == "__builtin_va_start") {
        expect(TOK_LPAREN, "Expected '(' after va_start");
        auto ap = parse_expr();
        expect(TOK_COMMA, "Expected ',' in va_start");
        Token last_param = expect(TOK_IDENTIFIER, "Expected parameter name in va_start");
        expect(TOK_RPAREN, "Expected ')' after va_start");
        auto addr_of = std::make_unique<UnaryOpNode>(last_param.line, TOK_AMP,
            std::make_unique<IdentifierNode>(last_param.line, last_param.text));
        auto plus_eight = std::make_unique<BinaryOpNode>(last_param.line, TOK_PLUS,
            std::move(addr_of), std::make_unique<IntLitNode>(last_param.line, 8));
        expr = std::make_unique<AssignNode>(t.line, std::move(ap), std::move(plus_eight));
      } else if (id_name == "va_arg" || id_name == "__builtin_va_arg") {
        expect(TOK_LPAREN, "Expected '(' after va_arg");
        Token ap_tok = expect(TOK_IDENTIFIER, "Expected va_list variable in va_arg");
        expect(TOK_COMMA, "Expected ',' in va_arg");
        DataType arg_type = parse_type_specifier();
        int ptr_level = 0;
        while (match(TOK_STAR)) ++ptr_level;
        (void)arg_type; (void)ptr_level;
        expect(TOK_RPAREN, "Expected ')' after va_arg");
        auto ap_read = std::make_unique<IdentifierNode>(t.line, ap_tok.text);
        auto ap_plus_8 = std::make_unique<BinaryOpNode>(t.line, TOK_PLUS,
            std::move(ap_read), std::make_unique<IntLitNode>(t.line, 8));
        auto ap_lval = std::make_unique<IdentifierNode>(t.line, ap_tok.text);
        auto assign = std::make_unique<AssignNode>(t.line, std::move(ap_lval), std::move(ap_plus_8));
        auto minus_8 = std::make_unique<BinaryOpNode>(t.line, TOK_MINUS,
            std::move(assign), std::make_unique<IntLitNode>(t.line, 8));
        expr = std::make_unique<UnaryOpNode>(t.line, TOK_STAR, std::move(minus_8));
      } else if (id_name == "va_end" || id_name == "__builtin_va_end") {
        expect(TOK_LPAREN, "Expected '(' after va_end");
        parse_expr();
        expect(TOK_RPAREN, "Expected ')' after va_end");
        expr = std::make_unique<IntLitNode>(t.line, 0);
      } else if (id_name == "va_copy" || id_name == "__builtin_va_copy") {
        expect(TOK_LPAREN, "Expected '(' after va_copy");
        auto dest = parse_expr();
        expect(TOK_COMMA, "Expected ',' in va_copy");
        auto src = parse_expr();
        expect(TOK_RPAREN, "Expected ')' after va_copy");
        expr = std::make_unique<AssignNode>(t.line, std::move(dest), std::move(src));
      } else {
        expect(TOK_LPAREN, "Expected '(' after function name");
        std::vector<std::unique_ptr<ASTNode>> args;
        if (peek().type != TOK_RPAREN) {
          while (true) {
            args.push_back(parse_expr());
            if (match(TOK_COMMA)) {
              continue;
            }
            break;
          }
        }
        expect(TOK_RPAREN, "Expected ')' after function arguments");
        expr = std::make_unique<CallNode>(t.line, t.text, std::move(args));
      }
    } else {
      expr = std::make_unique<IdentifierNode>(t.line, t.text);
    }
  } else if (t.type == TOK_LPAREN) {
    expr = parse_parenthesized([this] { return parse_expr(); },
                               "Expected '('",
                               "Expected ')' after expression");
  } else {
    parse_error(t, "Expected expression");
  }

  // Postfix array indexing, member access, calls, and postfix ++ / --
  while (true) {
    if (match(TOK_INC)) {
      bool valid_lvalue = (expr->getType() == ASTNodeType::Identifier) ||
                          (expr->getType() == ASTNodeType::ArrayAccess) ||
                          (expr->getType() == ASTNodeType::MemberAccess) ||
                          (expr->getType() == ASTNodeType::UnaryOp &&
                           static_cast<const UnaryOpNode *>(expr.get())->op == TOK_STAR);
      if (!valid_lvalue) {
        parse_error(peek(), "Operand of postfix ++ must be an lvalue");
      }
      expr = std::make_unique<UnaryOpNode>(expr->line, TOK_INC, std::move(expr), true /* is_postfix */);
    } else if (match(TOK_DEC)) {
      bool valid_lvalue = (expr->getType() == ASTNodeType::Identifier) ||
                          (expr->getType() == ASTNodeType::ArrayAccess) ||
                          (expr->getType() == ASTNodeType::MemberAccess) ||
                          (expr->getType() == ASTNodeType::UnaryOp &&
                           static_cast<const UnaryOpNode *>(expr.get())->op == TOK_STAR);
      if (!valid_lvalue) {
        parse_error(peek(), "Operand of postfix -- must be an lvalue");
      }
      expr = std::make_unique<UnaryOpNode>(expr->line, TOK_DEC, std::move(expr), true /* is_postfix */);
    } else if (match(TOK_LBRACKET)) {
      auto index = parse_expr();
      expect(TOK_RBRACKET, "Expected ']' after array index");
      expr = std::make_unique<ArrayAccessNode>(expr->line, std::move(expr),
                                               std::move(index));
    } else if (match(TOK_DOT)) {
      Token mem_tok = expect(TOK_IDENTIFIER, "Expected member name after '.'");
      expr = std::make_unique<MemberAccessNode>(expr->line, std::move(expr),
                                                mem_tok.text, false);
    } else if (match(TOK_ARROW)) {
      Token mem_tok = expect(TOK_IDENTIFIER, "Expected member name after '->'");
      expr = std::make_unique<MemberAccessNode>(expr->line, std::move(expr),
                                                mem_tok.text, true);
    } else if (peek().type == TOK_LPAREN && expr->getType() != ASTNodeType::IntLit) {
      expect(TOK_LPAREN, "Expected '(' after function pointer expression");
      std::vector<std::unique_ptr<ASTNode>> args;
      if (peek().type != TOK_RPAREN) {
        while (true) {
          args.push_back(parse_expr());
          if (match(TOK_COMMA)) {
            continue;
          }
          break;
        }
      }
      expect(TOK_RPAREN, "Expected ')' after arguments");
      expr = std::make_unique<CallNode>(expr->line, std::move(expr), std::move(args));
    } else {
      break;
    }
  }

  return expr;
}

std::unique_ptr<ASTNode> Parser::parse_unary() {
  Token t = peek();
  if (t.type == TOK_INC || t.type == TOK_DEC) {
    lexer_->next();
    auto expr = parse_unary();
    bool valid_lvalue = (expr->getType() == ASTNodeType::Identifier) ||
                        (expr->getType() == ASTNodeType::ArrayAccess) ||
                        (expr->getType() == ASTNodeType::MemberAccess) ||
                        (expr->getType() == ASTNodeType::UnaryOp &&
                         static_cast<const UnaryOpNode *>(expr.get())->op == TOK_STAR);
    if (!valid_lvalue) {
      parse_error(t, "Operand of prefix ++/-- must be an lvalue");
    }
    return std::make_unique<UnaryOpNode>(t.line, t.type, std::move(expr), false /* is_postfix */);
  }
  if (t.type == TOK_PLUS || t.type == TOK_MINUS || t.type == TOK_LOGIC_NOT ||
      t.type == TOK_TILDE || t.type == TOK_STAR || t.type == TOK_AMP) {
    lexer_->next();
    auto expr = parse_unary();
    if (t.type == TOK_AMP && expr && expr->getType() == ASTNodeType::Identifier) {
      const auto *id_node = static_cast<const IdentifierNode *>(expr.get());
      if (is_register_var(id_node->name)) {
        parse_error(t, "Address of register variable '" + id_node->name + "' requested");
      }
    }
    return std::make_unique<UnaryOpNode>(t.line, t.type, std::move(expr));
  }
  return parse_primary();
}

std::unique_ptr<ASTNode> Parser::parse_expr_precedence(int min_prec) {
  auto left = parse_unary();

  while (true) {
    Token t = peek();
    auto it = BINARY_OPS.find(t.type);
    if (it == BINARY_OPS.end() || it->second.precedence < min_prec) {
      break;
    }

    lexer_->next(); // Consume operator token
    int next_prec = it->second.right_associative ? it->second.precedence
                                                 : it->second.precedence + 1;
    auto right = parse_expr_precedence(next_prec);
    left = std::make_unique<BinaryOpNode>(t.line, t.type, std::move(left),
                                          std::move(right));
  }

  return left;
}

std::unique_ptr<ASTNode> Parser::parse_conditional() {
  auto cond = parse_expr_precedence(1);
  if (match(TOK_QUESTION)) {
    auto then_expr = parse_expr();
    expect(TOK_COLON, "Expected ':' in conditional expression");
    auto else_expr = parse_conditional();
    return std::make_unique<TernaryOpNode>(cond->line, std::move(cond),
                                           std::move(then_expr),
                                           std::move(else_expr));
  }
  return cond;
}

std::unique_ptr<ASTNode> Parser::parse_assignment() {
  auto left = parse_conditional();
  Token t = peek();
  TokenType compound_bin_op = TOK_UNKNOWN;
  switch (t.type) {
  case TOK_ADD_ASSIGN: compound_bin_op = TOK_PLUS; break;
  case TOK_SUB_ASSIGN: compound_bin_op = TOK_MINUS; break;
  case TOK_MUL_ASSIGN: compound_bin_op = TOK_STAR; break;
  case TOK_DIV_ASSIGN: compound_bin_op = TOK_SLASH; break;
  case TOK_MOD_ASSIGN: compound_bin_op = TOK_PERCENT; break;
  case TOK_AND_ASSIGN: compound_bin_op = TOK_AMP; break;
  case TOK_OR_ASSIGN:  compound_bin_op = TOK_PIPE; break;
  case TOK_XOR_ASSIGN: compound_bin_op = TOK_CARET; break;
  case TOK_SHL_ASSIGN: compound_bin_op = TOK_LSHIFT; break;
  case TOK_SHR_ASSIGN: compound_bin_op = TOK_RSHIFT; break;
  default: break;
  }

  if (match(TOK_ASSIGN)) {
    bool valid_lvalue = (left->getType() == ASTNodeType::Identifier) ||
                        (left->getType() == ASTNodeType::ArrayAccess) ||
                        (left->getType() == ASTNodeType::MemberAccess) ||
                        (left->getType() == ASTNodeType::UnaryOp &&
                         static_cast<const UnaryOpNode *>(left.get())->op == TOK_STAR);
    if (!valid_lvalue) {
      Token err_tok{TOK_UNKNOWN, "", left->line, 1, {0}};
      parse_error(err_tok, "Left side of assignment must be an identifier, array element, member access, or dereference");
    }
    auto right = parse_assignment();
    return std::make_unique<AssignNode>(left->line, std::move(left),
                                        std::move(right));
  } else if (compound_bin_op != TOK_UNKNOWN) {
    lexer_->next(); // consume compound operator
    bool valid_lvalue = (left->getType() == ASTNodeType::Identifier) ||
                        (left->getType() == ASTNodeType::ArrayAccess) ||
                        (left->getType() == ASTNodeType::MemberAccess) ||
                        (left->getType() == ASTNodeType::UnaryOp &&
                         static_cast<const UnaryOpNode *>(left.get())->op == TOK_STAR);
    if (!valid_lvalue) {
      Token err_tok{TOK_UNKNOWN, "", left->line, 1, {0}};
      parse_error(err_tok, "Left side of assignment must be an identifier, array element, member access, or dereference");
    }
    auto right = parse_assignment();
    auto cloned_left = clone_ast(left.get());
    auto bin_node = std::make_unique<BinaryOpNode>(left->line, compound_bin_op,
                                                   std::move(cloned_left), std::move(right));
    return std::make_unique<AssignNode>(left->line, std::move(left),
                                        std::move(bin_node));
  }
  return left;
}

std::unique_ptr<ASTNode> Parser::parse_expr() {
  return parse_assignment();
}
