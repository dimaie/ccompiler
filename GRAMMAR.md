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
    ::= func_def
      | struct_def
      | var_decl
      | if_stmt
      | for_stmt
      | while_stmt
      | do_while_stmt
      | switch_stmt
      | break_stmt
      | continue_stmt
      | goto_stmt
      | label_stmt
      | return_stmt
      | compound_stmt
      | expr_stmt
      | empty_stmt ;

struct_def
    ::= ( "struct" | "union" ) ( IDENTIFIER )? "{" member_decl* "}" ( "*"\* IDENTIFIER ( "[" ( INT_LIT )? "]" )? ( "=" ( expr | init_list ) )? )? ";" ;

member_decl
    ::= type_specifier "*"\* IDENTIFIER ( "[" INT_LIT "]" )? ";" ;

func_def
    ::= type_specifier "*"\* IDENTIFIER "(" ( "void" | param_list )? ")" compound_stmt ;

param_list
    ::= param ( "," param )* ;

param
    ::= type_specifier "*"\* IDENTIFIER
      | type_specifier "*"\* "(" "*" IDENTIFIER ")" "(" ( "void" | type_list )? ")" ;

type_list
    ::= type_specifier "*"\* ( "," type_specifier "*"\* )* ;

var_decl
    ::= type_specifier "*"\* IDENTIFIER ( "[" ( INT_LIT )? "]" )? ( "=" ( expr | init_list ) )? ";"
      | type_specifier "*"\* "(" "*" IDENTIFIER ")" "(" ( "void" | type_list )? ")" ( "=" expr )? ";" ;

init_list
    ::= "{" ( expr ( "," expr )* ( "," )? )? "}" ;

type_specifier
    ::= "void"
      | "int"
      | "short" ( "int" )?
      | "long" ( "long" )? ( "int" )?
      | ( "struct" | "union" ) IDENTIFIER ;

if_stmt
    ::= "if" "(" expr ")" statement ( "else" statement )? ;

for_stmt
    ::= "for" "(" ( var_decl | ( expr )? ";" ) ( expr )? ";" ( expr )? ")" statement ;

while_stmt
    ::= "while" "(" expr ")" statement ;

do_while_stmt
    ::= "do" statement "while" "(" expr ")" ";" ;

switch_stmt
    ::= "switch" "(" expr ")" "{" case_clause* "}" ;

case_clause
    ::= ( "case" expr ":" | "default" ":" ) statement* ;

break_stmt
    ::= "break" ";" ;

continue_stmt
    ::= "continue" ";" ;

goto_stmt
    ::= "goto" IDENTIFIER ";" ;

label_stmt
    ::= IDENTIFIER ":" ;

return_stmt
    ::= "return" ( expr )? ";" ;

compound_stmt
    ::= "{" statement* "}" ;

expr_stmt
    ::= expr ";" ;

empty_stmt
    ::= ";" ;

(* ========================================== *)
(* Expressions & Operator Precedence          *)
(* ========================================== *)

expr
    ::= assignment ;

assignment
    ::= ( IDENTIFIER | array_access | member_access | deref_expr ) "=" assignment
      | logical_or ;

deref_expr
    ::= "*" unary
      | "(" "*" unary ")" ;

member_access
    ::= primary ( "." | "->" ) IDENTIFIER ;

array_access
    ::= IDENTIFIER "[" expr "]" ( "[" expr "]" )* ;

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
    ::= ( "+" | "-" | "!" | "~" | "&" | "*" ) unary
      | primary ;

primary
    ::= primary_base ( "[" expr "]" | "." IDENTIFIER | "->" IDENTIFIER | "(" ( expr ( "," expr )* )? ")" )* ;

primary_base
    ::= IDENTIFIER
      | INT_LIT
      | "(" expr ")" ;
```

---

## 2. Operator Precedence and Associativity Table

Operators supported by the parser, ordered from highest to lowest precedence:

| Precedence | Operator | Description | Associativity | AST Node Type |
| :---: | :---: | :--- | :---: | :--- |
| **1 (Highest)** | `( )`, `[ ]`, `.`, `->` | Function call, array subscript, structure/union member access | Left-to-right | `AST_CALL`, `AST_ARRAY_ACCESS`, `AST_MEMBER_ACCESS` |
| **2** | `+`, `-`, `!`, `~`, `&`, `*` | Unary positive, negation, logical NOT, bitwise NOT, address-of, dereference | Right-to-left | `AST_UNARY_OP` |
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
| `IDENTIFIER` | `[a-zA-Z_][a-zA-Z0-9_]*` (not a keyword) | Variable/label identifier (up to 63 chars) |
| `INT_LIT` | `0[xX][0-9a-fA-F]+ \| [0-9]+` | Integer literal (decimal, hex, octal) |
| `"int"` | Keyword `int` (`TOK_KW_INT`) | Type specifier for variable declaration |
| `"short"` | Keyword `short` (`TOK_KW_SHORT`) | Short integer modifier / type specifier |
| `"long"` | Keyword `long` (`TOK_KW_LONG`) | Long integer modifier / type specifier |
| `"if"` | Keyword `if` (`TOK_KW_IF`) | Conditional statement |
| `"else"` | Keyword `else` (`TOK_KW_ELSE`) | Alternative branch for conditional |
| `"for"` | Keyword `for` (`TOK_KW_FOR`) | For loop iteration statement |
| `"while"` | Keyword `while` (`TOK_KW_WHILE`) | While loop iteration statement |
| `"do"` | Keyword `do` (`TOK_KW_DO`) | Do-while loop iteration statement |
| `"switch"` | Keyword `switch` (`TOK_KW_SWITCH`) | Switch statement |
| `"case"` | Keyword `case` (`TOK_KW_CASE`) | Case clause in switch |
| `"default"` | Keyword `default` (`TOK_KW_DEFAULT`) | Default clause in switch |
| `"break"` | Keyword `break` (`TOK_KW_BREAK`) | Break out of loop or switch |
| `"continue"` | Keyword `continue` (`TOK_KW_CONTINUE`) | Jump to next loop iteration |
| `"goto"` | Keyword `goto` (`TOK_KW_GOTO`) | Jump to user label |
| `"return"` | Keyword `return` (`TOK_KW_RETURN`) | Return statement |
| Punctuators | `(`, `)`, `{`, `}`, `;`, `:`, `=` | Grouping and syntax delimiters |
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
| `Program` | `program` | Array of top-level statement nodes (`statements`) |
| `Compound` | `compound_stmt` (`{ ... }`) | Array of enclosed statement nodes (`statements`) |
| `VarDecl` | `var_decl` (`int x;`, `int a[3];`) | Variable name (`name`), type (`data_type`), `is_array`, `array_size`, optional initializer (`init`) |
| `FuncDef` | `func_def` (`int foo(int x) { ... }`) | Return type (`return_type`), name (`name`), parameters (`params`), body (`body`) |
| `Call` | `primary` (`foo(x, 1)`) | Callee identifier (`callee`), argument expressions (`args`) |
| `ArrayAccess` | `primary` / `array_access` (`a[i]`) | Base array expression (`array`), index expression (`index`) |
| `InitList` | `init_list` (`{1, 2, 3}`) | List of element expressions (`elements`) |
| `If` | `if_stmt` (`if (...) ... else ...`) | `cond`, `then_branch`, optional `else_branch` |
| `For` | `for_stmt` (`for (...; ...; ...) ...`) | `init`, `cond`, `step`, `body` |
| `While` | `while_stmt` (`while (...) ...`) | `cond`, `body` |
| `DoWhile` | `do_while_stmt` (`do ... while (...);`) | `body`, `cond` |
| `Switch` | `switch_stmt` (`switch (...) { ... }`) | `cond`, array of `CaseNode` pointers (`cases`) |
| `Case` | `case_clause` (`case val: ...` / `default: ...`) | `value`, `is_default`, `statements` |
| `Break` | `break_stmt` (`break;`) | Source line (`line`) |
| `Continue` | `continue_stmt` (`continue;`) | Source line (`line`) |
| `Goto` | `goto_stmt` (`goto label;`) | Target label identifier (`label`) |
| `Label` | `label_stmt` (`label:`) | Label identifier (`name`) |
| `Return` | `return_stmt` (`return expr;`) | Return value expression (`expr`) |
| `Assign` | `assignment` (`id = expr`, `a[i] = expr`) | Target lvalue expression (`left`), value expression (`right`) |
| `BinaryOp` | Binary expressions (`+`, `-`, `*`, `&&`, etc.) | Operator token (`op`), `left`, `right` |
| `UnaryOp` | Prefix unary expressions (`-`, `!`, etc.) | Operator token (`op`), `expr` |
| `Identifier` | Variable reference (`IDENTIFIER`) | Name (`name`) |
| `IntLit` | Integer literal (`INT_LIT`) | 64-bit signed integer value (`val`) |

---

## 5. Lexer Capabilities vs Parser Limitations

The lexer ([`lexer.cpp`](file:///c:/Work/CCompiler/lexer.cpp)) scans a broader subset of C that is **not yet supported** by the recursive descent parser:

1. **Additional Keywords Lexed:** `auto`, `char`, `const`, `double`, `enum`, `extern`, `float`, `register`, `signed`, `sizeof`, `static`, `typedef`, `unsigned`, `volatile`.
2. **Literals Lexed:** String literals (`"..."`) and character literals (`'...'`) are scanned by the lexer, but rejected by `parse_primary`.
3. **Operators Lexed:** Increment/decrement (`++`, `--`), bitwise operators (`|`, `^`, `<<`, `>>`), and preprocessor hash (`#`) are tokenized by the lexer, but not accepted in expressions by the parser.
4. **Type Definitions & Enums:** `typedef` and `enum` are not yet implemented. (Structures, unions, member access `.` and `->`, pointers, and function pointers are fully supported).
