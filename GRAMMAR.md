# Supported Grammar Specification

This document formally specifies the grammar implemented and supported by the compiler front-end based on [`lexer.hpp`](file:///c:/Work/CCompiler/lexer.hpp), [`lexer.cpp`](file:///c:/Work/CCompiler/lexer.cpp), [`parser.hpp`](file:///c:/Work/CCompiler/parser.hpp), and modular parser sources (`parser_core.cpp`, `parser_decl.cpp`, `parser_expr.cpp`, `parser_stmt.cpp`).

---

## 1. Syntactic Grammar (EBNF)

Below is the Extended Backus–Naur Form (EBNF) representing the syntax accepted by the parser (`parser_parse_program`).

```ebnf
(* ========================================== *)
(* Program & Statements                       *)
(* ========================================== *)

program
    ::= statement* EOF ;

statement
    ::= var_decl
      | if_stmt
      | for_stmt
      | return_stmt
      | compound_stmt
      | expr_stmt ;

var_decl
    ::= "int" IDENTIFIER ( "=" expr )? ";" ;

if_stmt
    ::= "if" "(" expr ")" statement ( "else" statement )? ;

for_stmt
    ::= "for" "(" ( var_decl | ( expr )? ";" ) ( expr )? ";" ( expr )? ")" statement ;

return_stmt
    ::= "return" expr ";" ;

compound_stmt
    ::= "{" statement* "}" ;

expr_stmt
    ::= expr ";" ;

(* ========================================== *)
(* Expressions & Operator Precedence          *)
(* ========================================== *)

expr
    ::= assignment ;

assignment
    ::= IDENTIFIER "=" assignment
      | logical_or ;

logical_or
    ::= logical_and ( "||" logical_and )* ;

logical_and
    ::= equality ( "&&" equality )* ;

equality
    ::= relational ( ( "==" | "!=" ) relational )* ;

relational
    ::= additive ( ( "<" | "<=" | ">" | ">=" ) additive )* ;

additive
    ::= multiplicative ( ( "+" | "-" ) multiplicative )* ;

multiplicative
    ::= unary ( ( "*" | "/" | "%" ) unary )* ;

unary
    ::= ( "+" | "-" | "!" | "~" ) unary
      | primary ;

primary
    ::= IDENTIFIER
      | INT_LIT
      | "(" expr ")" ;
```

---

## 2. Operator Precedence and Associativity Table

Operators supported by the parser, ordered from highest to lowest precedence:

| Precedence | Operator | Description | Associativity | AST Node Type |
| :---: | :---: | :--- | :---: | :--- |
| **1 (Highest)** | `( )` | Parenthesized expression | N/A | Evaluated first |
| **2** | `+`, `-`, `!`, `~` | Unary positive, negation, logical NOT, bitwise NOT | Right-to-left | `AST_UNARY_OP` |
| **3** | `*`, `/`, `%` | Multiplication, division, modulo | Left-to-right | `AST_BINARY_OP` |
| **4** | `+`, `-` | Addition, subtraction | Left-to-right | `AST_BINARY_OP` |
| **5** | `<`, `<=`, `>`, `>=` | Relational comparisons | Left-to-right | `AST_BINARY_OP` |
| **6** | `==`, `!=` | Equality and inequality | Left-to-right | `AST_BINARY_OP` |
| **7** | `&&` | Logical AND | Left-to-right | `AST_BINARY_OP` |
| **8** | `\|\|` | Logical OR | Left-to-right | `AST_BINARY_OP` |
| **9 (Lowest)** | `=` | Assignment | Right-to-left | `AST_ASSIGN` |

---

## 3. Lexical Grammar

### 3.1 Terminals Accepted by the Parser

| Terminal | Lexical Pattern / Definition | Description |
| :--- | :--- | :--- |
| `IDENTIFIER` | `[a-zA-Z_][a-zA-Z0-9_]*` (not a keyword) | Variable identifier (up to 63 chars) |
| `INT_LIT` | `0[xX][0-9a-fA-F]+ \| [0-9]+` | Integer literal (decimal, hex, octal) |
| `"int"` | Keyword `int` (`TOK_KW_INT`) | Type specifier for variable declaration |
| `"if"` | Keyword `if` (`TOK_KW_IF`) | Conditional statement |
| `"else"` | Keyword `else` (`TOK_KW_ELSE`) | Alternative branch for conditional |
| `"for"` | Keyword `for` (`TOK_KW_FOR`) | For loop iteration statement |
| `"return"` | Keyword `return` (`TOK_KW_RETURN`) | Return statement |
| Punctuators | `(`, `)`, `{`, `}`, `;`, `=` | Grouping and syntax delimiters |
| Operators | `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `\|\|`, `!`, `~` | Arithmetic, relational, logical operators |

### 3.2 Whitespace and Comments (Ignored)

- **Whitespace:** Space (`' '`), tab (`'\t'`), carriage return (`'\r'`), newline (`'\n'`).
- **Line Comments:** `// ... \n`
- **Block Comments:** `/* ... */`

---

## 4. Parser AST Nodes (`ASTNodeType`)

The parser maps the grammar into the following AST node representations defined in [`parser.hpp`](file:///c:/Work/CCompiler/parser.hpp) and [`ast.hpp`](file:///c:/Work/CCompiler/ast.hpp):

| Node Type | Corresponding Grammar Construct | Stored Fields |
| :--- | :--- | :--- |
| `AST_PROGRAM` | `program` | Array of top-level statement nodes (`block.stmts`) |
| `AST_COMPOUND` | `compound_stmt` (`{ ... }`) | Array of enclosed statement nodes (`block.stmts`) |
| `AST_VAR_DECL` | `var_decl` (`int x [= expr];`) | Variable name (`var_decl.name`), optional initializer (`var_decl.init`) |
| `AST_IF` | `if_stmt` (`if (...) ... else ...`) | `if_stmt.cond`, `if_stmt.then_branch`, optional `if_stmt.else_branch` |
| `AST_RETURN` | `return_stmt` (`return expr;`) | Return value expression (`ret_stmt.expr`) |
| `AST_ASSIGN` | `assignment` (`id = expr`) | Target identifier (`binary.left`), value expression (`binary.right`) |
| `AST_BINARY_OP` | Binary expressions (`+`, `-`, `*`, `&&`, etc.) | Operator token (`binary.op`), left (`binary.left`), right (`binary.right`) |
| `AST_UNARY_OP` | Prefix unary expressions (`-`, `!`, etc.) | Operator token (`unary.op`), operand expression (`unary.expr`) |
| `AST_IDENTIFIER` | Variable reference (`IDENTIFIER`) | Name (`name`) |
| `AST_INT_LIT` | Integer literal (`INT_LIT`) | 64-bit signed integer value (`int_val`) |

---

## 5. Lexer Capabilities vs Parser Limitations

The lexer ([`lexer.cpp`](file:///c:/Work/CCompiler/lexer.cpp)) scans a broader subset of C that is **not yet supported** by the recursive descent parser:

1. **Additional Keywords Lexed (32 in total):** `auto`, `break`, `case`, `char`, `const`, `continue`, `default`, `do`, `double`, `enum`, `extern`, `float`, `for`, `goto`, `long`, `register`, `short`, `signed`, `sizeof`, `static`, `struct`, `switch`, `typedef`, `union`, `unsigned`, `void`, `volatile`, `while`.
2. **Literals Lexed:** String literals (`"..."`) and character literals (`'...'`) are scanned by the lexer, but rejected by `parse_primary`.
3. **Operators Lexed:** Increment/decrement (`++`, `--`), bitwise operators (`&`, `|`, `^`, `<<`, `>>`), member access (`.`, `->`), brackets (`[`, `]`), commas (`,`), and preprocessor hash (`#`) are tokenized by the lexer, but not accepted in expressions by the parser.
4. **Functions:** Function definitions, declarations, parameters, and function calls (`foo(...)`) are not yet part of the grammar. Top-level statements are parsed directly into the program body.
