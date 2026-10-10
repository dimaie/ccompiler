/**
 * @file lexer.hpp
 * @brief Lexical analyzer (scanner) interface for the C compiler frontend.
 *
 * Defines the token types, token representation, lexer class,
 * and scanning routines.
 */

#ifndef LEXER_HPP
#define LEXER_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

/**
 * @brief Enumeration of all token categories recognized by the lexer.
 */
enum TokenType {
  /* Special Tokens */
  TOK_EOF = 0, /**< End of input stream */
  TOK_UNKNOWN, /**< Unrecognized / invalid character sequence */

  /* Literals & Identifiers */
  TOK_IDENTIFIER, /**< Identifier: [a-zA-Z_][a-zA-Z0-9_]* */
  TOK_INT_LIT,   /**< Integer constant: decimal, hex (0x...), or octal (0...) */
  TOK_FLOAT_LIT, /**< Floating-point constant */
  TOK_CHAR_LIT,  /**< Character constant: 'c' */
  TOK_STRING_LIT, /**< String literal: "..." */

  /* Keywords (32 standard C keywords) */
  TOK_KW_AUTO,
  TOK_KW_BREAK,
  TOK_KW_CASE,
  TOK_KW_CHAR,
  TOK_KW_CONST,
  TOK_KW_CONTINUE,
  TOK_KW_DEFAULT,
  TOK_KW_DO,
  TOK_KW_DOUBLE,
  TOK_KW_ELSE,
  TOK_KW_ENUM,
  TOK_KW_EXTERN,
  TOK_KW_FLOAT,
  TOK_KW_FOR,
  TOK_KW_GOTO,
  TOK_KW_IF,
  TOK_KW_INT,
  TOK_KW_LONG,
  TOK_KW_REGISTER,
  TOK_KW_RETURN,
  TOK_KW_SHORT,
  TOK_KW_SIGNED,
  TOK_KW_SIZEOF,
  TOK_KW_STATIC,
  TOK_KW_STRUCT,
  TOK_KW_SWITCH,
  TOK_KW_TYPEDEF,
  TOK_KW_UNION,
  TOK_KW_UNSIGNED,
  TOK_KW_VOID,
  TOK_KW_VOLATILE,
  TOK_KW_WHILE,

  /* Punctuation & Delimiters */
  TOK_LPAREN,    /**< '(' Grouping parenthesis */
  TOK_RPAREN,    /**< ')' Grouping parenthesis */
  TOK_LBRACE,    /**< '{' Block opening delimiter */
  TOK_RBRACE,    /**< '}' Block closing delimiter */
  TOK_LBRACKET,  /**< '[' Subscript opening bracket */
  TOK_RBRACKET,  /**< ']' Subscript closing bracket */
  TOK_SEMICOLON, /**< ';' Statement terminator */
  TOK_COLON,     /**< ':' Colon delimiter */
  TOK_COMMA,     /**< ',' List delimiter */
  TOK_DOT,       /**< '.' Structure member access */
  TOK_ARROW,     /**< '->' Structure pointer member access */
  TOK_HASH,      /**< '#' Preprocessor directive marker */

  /* Operators */
  TOK_ASSIGN,     /**< '=' Assignment operator */
  TOK_PLUS,       /**< '+' Addition or unary positive */
  TOK_MINUS,      /**< '-' Subtraction or unary negation */
  TOK_STAR,       /**< '*' Multiplication or pointer dereference */
  TOK_SLASH,      /**< '/' Division operator */
  TOK_PERCENT,    /**< '%' Modulo remainder operator */
  TOK_INC,        /**< '++' Increment operator */
  TOK_DEC,        /**< '--' Decrement operator */
  TOK_EQUAL,      /**< '==' Equality comparison */
  TOK_NOT_EQUAL,  /**< '!=' Inequality comparison */
  TOK_LESS,       /**< '<' Less-than comparison */
  TOK_GREATER,    /**< '>' Greater-than comparison */
  TOK_LESS_EQ,    /**< '<=' Less-than-or-equal comparison */
  TOK_GREATER_EQ, /**< '>=' Greater-than-or-equal comparison */
  TOK_LOGIC_AND,  /**< '&&' Short-circuit logical AND */
  TOK_LOGIC_OR,   /**< '||' Short-circuit logical OR */
  TOK_LOGIC_NOT,  /**< '!' Logical NOT */
  TOK_AMP,        /**< '&' Bitwise AND or address-of */
  TOK_PIPE,       /**< '|' Bitwise OR */
  TOK_CARET,      /**< '^' Bitwise XOR */
  TOK_TILDE,      /**< '~' Bitwise NOT (one's complement) */
  TOK_LSHIFT,     /**< '<<' Bitwise left shift */
  TOK_RSHIFT      /**< '>>' Bitwise right shift */
};

/**
 * @brief Representation of an individual token produced by the scanner.
 */
struct Token {
  TokenType type{TOK_EOF}; /**< Classification category of the token */
  char text[128]{};        /**< Literal character text of the token */
  int line{1};             /**< 1-based source line number where token starts */
  int col{1}; /**< 1-based source column number where token starts */
  union {
    int64_t int_val;  /**< Evaluated 64-bit integer value (for TOK_INT_LIT,
                         TOK_CHAR_LIT) */
    double float_val; /**< Evaluated IEEE double value (for TOK_FLOAT_LIT) */
  } value{0};
};

/**
 * @brief State and scanner engine for lexical analysis.
 */

class Lexer {
public:
  static constexpr size_t LOOKAHEAD_BUFFER = 4;

  Lexer();
  explicit Lexer(const char *source);
  explicit Lexer(std::string_view source);

  void init(const char *source);
  void init(std::string_view source);

  Token next();
  Token peek(size_t k = 1);

  const char *getSource() const { return src_; }
  size_t getCursor() const { return cursor_; }
  int getLine() const { return line_; }
  int getCol() const { return col_; }

private:
  char peek_char() const;
  char peek_next_char() const;
  char advance_char();
  void skip_whitespace_and_comments();
  char parse_escape_sequence();
  Token scan_token();

  const char *src_{""};
  size_t cursor_{0};
  int line_{1};
  int col_{1};
  Token lookahead_[LOOKAHEAD_BUFFER]{};
  int lookahead_count_{0};
};

const char *token_name(TokenType t);

#endif // LEXER_HPP
