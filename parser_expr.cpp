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
    if (peek().type == TOK_LPAREN) {
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

  // Postfix array indexing: arr[index] or indirect function calls (expr)(args)
  while (true) {
    if (match(TOK_LBRACKET)) {
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
  if (t.type == TOK_PLUS || t.type == TOK_MINUS || t.type == TOK_LOGIC_NOT ||
      t.type == TOK_TILDE || t.type == TOK_STAR || t.type == TOK_AMP) {
    lexer_->next();
    auto expr = parse_unary();
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

std::unique_ptr<ASTNode> Parser::parse_assignment() {
  auto left = parse_expr_precedence(1);
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
  }
  return left;
}

std::unique_ptr<ASTNode> Parser::parse_expr() {
  return parse_assignment();
}
