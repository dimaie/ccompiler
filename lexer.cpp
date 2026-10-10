/**
 * @file lexer.cpp
 * @brief Lexical analyzer (scanner) implementation for the C compiler frontend.
 *
 * Converts a C source string into a stream of discrete tokens.
 */

#include "lexer.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct KeywordMap {
  const char *str;
  TokenType type;
};

constexpr KeywordMap KEYWORDS[] = {
    {"auto", TOK_KW_AUTO},         {"break", TOK_KW_BREAK},
    {"case", TOK_KW_CASE},         {"char", TOK_KW_CHAR},
    {"const", TOK_KW_CONST},       {"continue", TOK_KW_CONTINUE},
    {"default", TOK_KW_DEFAULT},   {"do", TOK_KW_DO},
    {"double", TOK_KW_DOUBLE},     {"else", TOK_KW_ELSE},
    {"enum", TOK_KW_ENUM},         {"extern", TOK_KW_EXTERN},
    {"float", TOK_KW_FLOAT},       {"for", TOK_KW_FOR},
    {"goto", TOK_KW_GOTO},         {"if", TOK_KW_IF},
    {"int", TOK_KW_INT},           {"long", TOK_KW_LONG},
    {"register", TOK_KW_REGISTER}, {"return", TOK_KW_RETURN},
    {"short", TOK_KW_SHORT},       {"signed", TOK_KW_SIGNED},
    {"sizeof", TOK_KW_SIZEOF},     {"static", TOK_KW_STATIC},
    {"struct", TOK_KW_STRUCT},     {"switch", TOK_KW_SWITCH},
    {"typedef", TOK_KW_TYPEDEF},   {"union", TOK_KW_UNION},
    {"unsigned", TOK_KW_UNSIGNED}, {"void", TOK_KW_VOID},
    {"volatile", TOK_KW_VOLATILE}, {"while", TOK_KW_WHILE},
    {nullptr, TOK_UNKNOWN}};

} // namespace

Lexer::Lexer() : Lexer("") {}

Lexer::Lexer(const char *source) { init(source); }

Lexer::Lexer(std::string_view source) { init(source); }

void Lexer::init(const char *source) {
  src_ = source ? source : "";
  cursor_ = 0;
  line_ = 1;
  col_ = 1;
  lookahead_count_ = 0;
}

void Lexer::init(std::string_view source) { init(source.data()); }

char Lexer::peek_char() const { return src_[cursor_]; }

char Lexer::peek_next_char() const {
  if (src_[cursor_] == '\0') {
    return '\0';
  }
  return src_[cursor_ + 1];
}

char Lexer::advance_char() {
  char c = src_[cursor_];
  if (c != '\0') {
    cursor_++;
    if (c == '\n') {
      line_++;
      col_ = 1;
    } else {
      col_++;
    }
  }
  return c;
}

void Lexer::skip_whitespace_and_comments() {
  while (true) {
    char c = peek_char();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance_char();
    } else if (c == '/' && peek_next_char() == '/') {
      while (peek_char() != '\n' && peek_char() != '\0') {
        advance_char();
      }
    } else if (c == '/' && peek_next_char() == '*') {
      advance_char(); // consume '/'
      advance_char(); // consume '*'
      while (peek_char() != '\0') {
        if (peek_char() == '*' && peek_next_char() == '/') {
          advance_char(); // consume '*'
          advance_char(); // consume '/'
          break;
        }
        advance_char();
      }
    } else {
      break;
    }
  }
}

char Lexer::parse_escape_sequence() {
  char esc = advance_char();
  switch (esc) {
  case 'n':
    return '\n';
  case 't':
    return '\t';
  case 'r':
    return '\r';
  case '0':
    return '\0';
  case '\\':
    return '\\';
  case '\'':
    return '\'';
  case '\"':
    return '\"';
  default:
    return esc;
  }
}

Token Lexer::scan_token() {
  skip_whitespace_and_comments();

  Token tok{};
  tok.line = line_;
  tok.col = col_;

  char c = peek_char();

  // End-of-file check
  if (c == '\0') {
    tok.type = TOK_EOF;
    std::snprintf(tok.text, sizeof(tok.text), "<EOF>");
    return tok;
  }

  // 1. Identifiers and Keywords: [a-zA-Z_][a-zA-Z0-9_]*
  if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
    size_t len = 0;
    while ((std::isalnum(static_cast<unsigned char>(peek_char())) ||
            peek_char() == '_') &&
           len < sizeof(tok.text) - 1) {
      tok.text[len++] = advance_char();
    }
    tok.text[len] = '\0';
    tok.type = TOK_IDENTIFIER;

    for (int i = 0; KEYWORDS[i].str != nullptr; ++i) {
      if (std::strcmp(KEYWORDS[i].str, tok.text) == 0) {
        tok.type = KEYWORDS[i].type;
        break;
      }
    }
    return tok;
  }

  // 2. Numeric Literals: Decimal, Hexadecimal (0x...), Octal (0...)
  if (std::isdigit(static_cast<unsigned char>(c))) {
    size_t len = 0;
    bool is_hex = false;

    if (c == '0' && (peek_next_char() == 'x' || peek_next_char() == 'X')) {
      is_hex = true;
      tok.text[len++] = advance_char();
      tok.text[len++] = advance_char();
    }

    while (len < sizeof(tok.text) - 1) {
      char p = peek_char();
      if (is_hex ? std::isxdigit(static_cast<unsigned char>(p))
                 : std::isdigit(static_cast<unsigned char>(p))) {
        tok.text[len++] = advance_char();
      } else {
        break;
      }
    }
    tok.text[len] = '\0';
    tok.type = TOK_INT_LIT;
    tok.value.int_val = std::strtoll(tok.text, nullptr, 0);
    return tok;
  }

  // 3. String Literals: "..."
  if (c == '"') {
    advance_char();
    size_t len = 0;
    while (peek_char() != '"' && peek_char() != '\0' &&
           len < sizeof(tok.text) - 1) {
      if (peek_char() == '\\') {
        advance_char();
        tok.text[len++] = parse_escape_sequence();
      } else {
        tok.text[len++] = advance_char();
      }
    }
    if (peek_char() == '"') {
      advance_char();
    }
    tok.text[len] = '\0';
    tok.type = TOK_STRING_LIT;
    return tok;
  }

  // 4. Character Literals: '...'
  if (c == '\'') {
    advance_char();
    char ch_val = advance_char();
    if (ch_val == '\\') {
      ch_val = parse_escape_sequence();
    }
    if (peek_char() == '\'') {
      advance_char();
    }
    tok.type = TOK_CHAR_LIT;
    tok.value.int_val = ch_val;
    std::snprintf(tok.text, sizeof(tok.text), "'%c'", ch_val);
    return tok;
  }

  // 5. Operators and Delimiters
  advance_char();
  tok.text[0] = c;
  tok.text[1] = '\0';

  switch (c) {
  case '(':
    tok.type = TOK_LPAREN;
    break;
  case ')':
    tok.type = TOK_RPAREN;
    break;
  case '{':
    tok.type = TOK_LBRACE;
    break;
  case '}':
    tok.type = TOK_RBRACE;
    break;
  case '[':
    tok.type = TOK_LBRACKET;
    break;
  case ']':
    tok.type = TOK_RBRACKET;
    break;
  case ';':
    tok.type = TOK_SEMICOLON;
    break;
  case ':':
    tok.type = TOK_COLON;
    break;
  case ',':
    tok.type = TOK_COMMA;
    break;
  case '#':
    tok.type = TOK_HASH;
    break;
  case '~':
    tok.type = TOK_TILDE;
    break;
  case '^':
    tok.type = TOK_CARET;
    break;

  case '+':
    if (peek_char() == '+') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "++");
      tok.type = TOK_INC;
    } else {
      tok.type = TOK_PLUS;
    }
    break;

  case '-':
    if (peek_char() == '-') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "--");
      tok.type = TOK_DEC;
    } else if (peek_char() == '>') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "->");
      tok.type = TOK_ARROW;
    } else {
      tok.type = TOK_MINUS;
    }
    break;

  case '=':
    if (peek_char() == '=') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "==");
      tok.type = TOK_EQUAL;
    } else {
      tok.type = TOK_ASSIGN;
    }
    break;

  case '!':
    if (peek_char() == '=') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "!=");
      tok.type = TOK_NOT_EQUAL;
    } else {
      tok.type = TOK_LOGIC_NOT;
    }
    break;

  case '<':
    if (peek_char() == '<') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "<<");
      tok.type = TOK_LSHIFT;
    } else if (peek_char() == '=') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "<=");
      tok.type = TOK_LESS_EQ;
    } else {
      tok.type = TOK_LESS;
    }
    break;

  case '>':
    if (peek_char() == '>') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), ">>");
      tok.type = TOK_RSHIFT;
    } else if (peek_char() == '=') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), ">=");
      tok.type = TOK_GREATER_EQ;
    } else {
      tok.type = TOK_GREATER;
    }
    break;

  case '&':
    if (peek_char() == '&') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "&&");
      tok.type = TOK_LOGIC_AND;
    } else {
      tok.type = TOK_AMP;
    }
    break;

  case '|':
    if (peek_char() == '|') {
      advance_char();
      std::snprintf(tok.text, sizeof(tok.text), "||");
      tok.type = TOK_LOGIC_OR;
    } else {
      tok.type = TOK_PIPE;
    }
    break;

  case '*':
    tok.type = TOK_STAR;
    break;
  case '/':
    tok.type = TOK_SLASH;
    break;
  case '%':
    tok.type = TOK_PERCENT;
    break;
  case '.':
    tok.type = TOK_DOT;
    break;

  default:
    tok.type = TOK_UNKNOWN;
    break;
  }

  return tok;
}

Token Lexer::next() {
  if (lookahead_count_ > 0) {
    Token tok = lookahead_[0];
    lookahead_[0] = lookahead_[1];
    lookahead_count_--;
    return tok;
  }
  return scan_token();
}

Token Lexer::peek() {
  if (lookahead_count_ == 0) {
    lookahead_[0] = scan_token();
    lookahead_count_ = 1;
  }
  return lookahead_[0];
}

Token Lexer::peek2() {
  if (lookahead_count_ == 0) {
    lookahead_[0] = scan_token();
    lookahead_[1] = scan_token();
    lookahead_count_ = 2;
  } else if (lookahead_count_ == 1) {
    lookahead_[1] = scan_token();
    lookahead_count_ = 2;
  }
  return lookahead_[1];
}

const char *token_name(TokenType t) {
  switch (t) {
  case TOK_EOF:
    return "EOF";
  case TOK_UNKNOWN:
    return "UNKNOWN";
  case TOK_IDENTIFIER:
    return "IDENTIFIER";
  case TOK_INT_LIT:
    return "INT_LIT";
  case TOK_FLOAT_LIT:
    return "FLOAT_LIT";
  case TOK_CHAR_LIT:
    return "CHAR_LIT";
  case TOK_STRING_LIT:
    return "STRING_LIT";

  case TOK_KW_AUTO:
    return "auto";
  case TOK_KW_BREAK:
    return "break";
  case TOK_KW_CASE:
    return "case";
  case TOK_KW_CHAR:
    return "char";
  case TOK_KW_CONST:
    return "const";
  case TOK_KW_CONTINUE:
    return "continue";
  case TOK_KW_DEFAULT:
    return "default";
  case TOK_KW_DO:
    return "do";
  case TOK_KW_DOUBLE:
    return "double";
  case TOK_KW_ELSE:
    return "else";
  case TOK_KW_ENUM:
    return "enum";
  case TOK_KW_EXTERN:
    return "extern";
  case TOK_KW_FLOAT:
    return "float";
  case TOK_KW_FOR:
    return "for";
  case TOK_KW_GOTO:
    return "goto";
  case TOK_KW_IF:
    return "if";
  case TOK_KW_INT:
    return "int";
  case TOK_KW_LONG:
    return "long";
  case TOK_KW_REGISTER:
    return "register";
  case TOK_KW_RETURN:
    return "return";
  case TOK_KW_SHORT:
    return "short";
  case TOK_KW_SIGNED:
    return "signed";
  case TOK_KW_SIZEOF:
    return "sizeof";
  case TOK_KW_STATIC:
    return "static";
  case TOK_KW_STRUCT:
    return "struct";
  case TOK_KW_SWITCH:
    return "switch";
  case TOK_KW_TYPEDEF:
    return "typedef";
  case TOK_KW_UNION:
    return "union";
  case TOK_KW_UNSIGNED:
    return "unsigned";
  case TOK_KW_VOID:
    return "void";
  case TOK_KW_VOLATILE:
    return "volatile";
  case TOK_KW_WHILE:
    return "while";

  case TOK_LPAREN:
    return "(";
  case TOK_RPAREN:
    return ")";
  case TOK_LBRACE:
    return "{";
  case TOK_RBRACE:
    return "}";
  case TOK_LBRACKET:
    return "[";
  case TOK_RBRACKET:
    return "]";
  case TOK_SEMICOLON:
    return ";";
  case TOK_COMMA:
    return ",";
  case TOK_DOT:
    return ".";
  case TOK_ARROW:
    return "->";
  case TOK_HASH:
    return "#";

  case TOK_ASSIGN:
    return "=";
  case TOK_PLUS:
    return "+";
  case TOK_MINUS:
    return "-";
  case TOK_STAR:
    return "*";
  case TOK_SLASH:
    return "/";
  case TOK_PERCENT:
    return "%";
  case TOK_INC:
    return "++";
  case TOK_DEC:
    return "--";
  case TOK_EQUAL:
    return "==";
  case TOK_NOT_EQUAL:
    return "!=";
  case TOK_LESS:
    return "<";
  case TOK_GREATER:
    return ">";
  case TOK_LESS_EQ:
    return "<=";
  case TOK_GREATER_EQ:
    return ">=";
  case TOK_LOGIC_AND:
    return "&&";
  case TOK_LOGIC_OR:
    return "||";
  case TOK_LOGIC_NOT:
    return "!";
  case TOK_AMP:
    return "&";
  case TOK_PIPE:
    return "|";
  case TOK_CARET:
    return "^";
  case TOK_TILDE:
    return "~";
  case TOK_LSHIFT:
    return "<<";
  case TOK_RSHIFT:
    return ">>";
  default:
    return "<UNKNOWN>";
  }
}
