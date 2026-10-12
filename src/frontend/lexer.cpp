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
#include <stdexcept>

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
    {"volatile", TOK_KW_VOLATILE}, {"__volatile__", TOK_KW_VOLATILE},
    {"__volatile", TOK_KW_VOLATILE},
    {"asm", TOK_KW_ASM},           {"__asm__", TOK_KW_ASM},
    {"__asm", TOK_KW_ASM},
    {"while", TOK_KW_WHILE},
    {"__cdecl", TOK_KW_CDECL},     {"_cdecl", TOK_KW_CDECL},
    {"cdecl", TOK_KW_CDECL},
    {"__stdcall", TOK_KW_STDCALL}, {"_stdcall", TOK_KW_STDCALL},
    {"stdcall", TOK_KW_STDCALL},
    {"__fastcall", TOK_KW_FASTCALL}, {"_fastcall", TOK_KW_FASTCALL},
    {"fastcall", TOK_KW_FASTCALL},
    {nullptr, TOK_UNKNOWN}};

} // namespace

Lexer::Lexer() : Lexer("") {}

Lexer::Lexer(const char *source) { init(source); }

Lexer::Lexer(std::string_view source) { init(source); }

void Lexer::init(const char *source) {
  if (source != owned_src_.c_str()) {
    owned_src_.clear();
  }
  src_ = source ? source : "";
  cursor_ = 0;
  line_ = 1;
  col_ = 1;
  current_file_id_ = 1;
  current_filename_.clear();
  file_table_.clear();
  file_id_map_.clear();
  lookahead_count_ = 0;

  // Skip UTF-8 BOM if present (0xEF, 0xBB, 0xBF)
  if (static_cast<unsigned char>(src_[0]) == 0xEF &&
      static_cast<unsigned char>(src_[1]) == 0xBB &&
      static_cast<unsigned char>(src_[2]) == 0xBF) {
    cursor_ = 3;
  }
}

void Lexer::setFilename(const std::string &name) {
  std::string norm = name;
  for (char &c : norm) {
    if (c == '\\') c = '/';
  }
  current_filename_ = norm;
  auto it = file_id_map_.find(norm);
  if (it != file_id_map_.end()) {
    current_file_id_ = it->second;
  } else {
    file_table_.push_back(norm);
    current_file_id_ = static_cast<int>(file_table_.size());
    file_id_map_[norm] = current_file_id_;
  }
}

void Lexer::init(std::string_view source) {
  owned_src_ = std::string(source);
  init(owned_src_.c_str());
}

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

void Lexer::parse_line_directive() {
  advance_char(); // consume '#'
  while (peek_char() == ' ' || peek_char() == '\t') advance_char();

  // Check for "line"
  if (peek_char() == 'l' && peek_next_char() == 'i') {
    std::string word;
    while (std::isalpha(static_cast<unsigned char>(peek_char()))) {
      word += advance_char();
    }
    if (word != "line") {
      while (peek_char() != '\n' && peek_char() != '\0') advance_char();
      if (peek_char() == '\n') advance_char();
      return;
    }
    while (peek_char() == ' ' || peek_char() == '\t') advance_char();
  }

  // Parse line number
  if (!std::isdigit(static_cast<unsigned char>(peek_char()))) {
    while (peek_char() != '\n' && peek_char() != '\0') advance_char();
    if (peek_char() == '\n') advance_char();
    return;
  }

  int new_line = 0;
  while (std::isdigit(static_cast<unsigned char>(peek_char()))) {
    new_line = new_line * 10 + (advance_char() - '0');
  }

  while (peek_char() == ' ' || peek_char() == '\t') advance_char();

  // Optional filename: "filename"
  std::string new_filename;
  if (peek_char() == '"') {
    advance_char(); // consume '"'
    while (peek_char() != '"' && peek_char() != '\n' && peek_char() != '\0') {
      if (peek_char() == '\\') {
        advance_char(); // consume '\'
        if (peek_char() == '\\') {
          new_filename += '/';
          advance_char();
        } else if (peek_char() == '"') {
          new_filename += '"';
          advance_char();
        } else {
          new_filename += '/';
        }
      } else {
        new_filename += advance_char();
      }
    }
    if (peek_char() == '"') advance_char(); // consume '"'
  }

  // Consume remainder of line (e.g. trailing flags)
  while (peek_char() != '\n' && peek_char() != '\0') {
    advance_char();
  }
  if (peek_char() == '\n') {
    advance_char(); // consume '\n'
  }

  // Set line number and filename for the NEXT line of code
  line_ = new_line;
  col_ = 1;
  if (!new_filename.empty()) {
    setFilename(new_filename);
  }
}

void Lexer::skip_whitespace_and_comments() {
  bool at_line_start = (col_ == 1);
  while (true) {
    char c = peek_char();
    if (c == ' ' || c == '\t' || c == '\r') {
      advance_char();
    } else if (c == '\n') {
      advance_char();
      at_line_start = true;
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
        if (peek_char() == '\n') {
          advance_char();
          at_line_start = true;
        } else {
          advance_char();
        }
      }
    } else if (c == '#' && at_line_start) {
      parse_line_directive();
      at_line_start = true;
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
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'v':
    return '\v';
  case 'x': {
    int val = 0;
    while (std::isxdigit(static_cast<unsigned char>(peek_char()))) {
      char h = advance_char();
      val = val * 16 + (std::isdigit(static_cast<unsigned char>(h))
                            ? (h - '0')
                            : (std::tolower(static_cast<unsigned char>(h)) - 'a' + 10));
    }
    return static_cast<char>(val);
  }
  default:
    return esc;
  }
}

Token Lexer::scan_token() {
  skip_whitespace_and_comments();

  Token tok{};
  tok.line = line_;
  tok.col = col_;
  tok.file_id = current_file_id_;

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
    while (std::isalnum(static_cast<unsigned char>(peek_char())) ||
           peek_char() == '_') {
      char ch = advance_char();
      if (len < sizeof(tok.text) - 1) {
        tok.text[len++] = ch;
      }
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

    while (true) {
      char p = peek_char();
      if (is_hex ? std::isxdigit(static_cast<unsigned char>(p))
                 : std::isdigit(static_cast<unsigned char>(p))) {
        char ch = advance_char();
        if (len < sizeof(tok.text) - 1) {
          tok.text[len++] = ch;
        }
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
    bool terminated = false;
    while (peek_char() != '\0') {
      if (peek_char() == '"') {
        advance_char();
        terminated = true;
        break;
      }
      if (peek_char() == '\\') {
        advance_char();
        char esc = parse_escape_sequence();
        if (len < sizeof(tok.text) - 1) {
          tok.text[len++] = esc;
        }
      } else {
        char ch = advance_char();
        if (len < sizeof(tok.text) - 1) {
          tok.text[len++] = ch;
        }
      }
    }
    tok.text[len] = '\0';
    tok.type = terminated ? TOK_STRING_LIT : TOK_UNKNOWN;
    return tok;
  }

  // 4. Character Literals: '...'
  if (c == '\'') {
    advance_char();
    if (peek_char() == '\0' || peek_char() == '\'') {
      tok.type = TOK_UNKNOWN;
      if (peek_char() == '\'') advance_char();
      return tok;
    }
    char ch_val = advance_char();
    if (ch_val == '\\') {
      ch_val = parse_escape_sequence();
    }
    if (peek_char() == '\'') {
      advance_char();
      tok.type = TOK_CHAR_LIT;
      tok.value.int_val = static_cast<unsigned char>(ch_val);
      std::snprintf(tok.text, sizeof(tok.text), "'%c'", ch_val);
      return tok;
    }
    tok.type = TOK_UNKNOWN;
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
  case '?':
    tok.type = TOK_QUESTION;
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
    if (peek_char() == '.' && peek_next_char() == '.') {
      advance_char();
      advance_char();
      tok.text[1] = '.';
      tok.text[2] = '.';
      tok.text[3] = '\0';
      tok.type = TOK_ELLIPSIS;
    } else {
      tok.type = TOK_DOT;
    }
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
    for (int i = 0; i < lookahead_count_ - 1; ++i) {
      lookahead_[i] = lookahead_[i + 1];
    }
    lookahead_count_--;
    return tok;
  }
  return scan_token();
}

Token Lexer::peek(size_t k) {
  if (k < 1 || k > LOOKAHEAD_BUFFER) {
    throw std::out_of_range("Lookahead index out of range (must be 1..4)");
  }
  while (lookahead_count_ < static_cast<int>(k)) {
    lookahead_[lookahead_count_++] = scan_token();
  }
  return lookahead_[k - 1];
}

const char *token_name(TokenType t) {
  // Check keywords from KEYWORDS map
  if (t >= TOK_KW_AUTO && t <= TOK_KW_FASTCALL) {
    for (int i = 0; KEYWORDS[i].str != nullptr; ++i) {
      if (KEYWORDS[i].type == t) {
        return KEYWORDS[i].str;
      }
    }
  }

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
  case TOK_COLON:
    return ":";
  case TOK_QUESTION:
    return "?";
  case TOK_COMMA:
    return ",";
  case TOK_DOT:
    return ".";
  case TOK_ELLIPSIS:
    return "...";
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
