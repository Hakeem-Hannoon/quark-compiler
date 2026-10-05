# Quark

A small compiled language. `quarkc` reads a `.qr` file, checks it, lowers it to
C, and hands that to the host C compiler to produce a native executable.

```
$ cat examples/1.qr
let x = 10;
let y = x * 2;
print(y);

$ make
$ ./build/quarkc examples/1.qr -o hello --run
20
```

## Building

```
make          # builds build/quarkc
make test     # builds, then runs the end-to-end suite
make clean
```

`CMakeLists.txt` is kept in sync for anyone who prefers cmake:

```
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Requires a C++17 compiler to build `quarkc`, and a C compiler on `PATH` at
compile time for the backend (`cc`, or whatever `$CC` points at).

## Usage

```
quarkc <file.qr> [options]

  -o <path>       Write the executable to <path> (default: the input's stem)
  --emit-tokens   Print the token stream and stop
  --emit-ast      Draw the syntax tree as text and stop
  --emit-svg      Draw the syntax tree as an SVG and stop
  --emit-dot      Draw the syntax tree as Graphviz DOT and stop
  --emit-c        Print the generated C and stop
  --keep-c        Keep the generated C next to the executable
  --run           Run the executable after building it
  -h, --help      Show this message
```

## Visualizing the AST

Three renderings of the same tree, all colour-coded by node kind.

`--emit-ast` draws it in the terminal, with no dependencies:

```
$ ./build/quarkc examples/1.qr --emit-ast
Program
├─ Let x
│  └─ Integer 10
├─ Let y
│  └─ Binary *
│     ├─ Identifier x
│     └─ Integer 2
└─ ExprStmt
   └─ Call print
      └─ Identifier y
```

`--emit-svg` writes a standalone SVG — quarkc does the tree layout itself, so
this needs nothing installed. It adapts to light and dark mode, and each box
carries its source line as a tooltip:

```
$ ./build/quarkc examples/1.qr --emit-svg > tree.svg && open tree.svg
```

`--emit-dot` writes Graphviz DOT, if you would rather use graphviz's layout
(`brew install graphviz`):

```
$ ./build/quarkc examples/1.qr --emit-dot | dot -Tpng -o tree.png
```

## The language

Two types (32-bit `int` and `bool`), one scope (global), one builtin (`print`).

```
let x = 10;             // declare; x is an int because 10 is
x = x + 1;              // assign to something already declared
let small = x < 100;    // small is a bool
print(x * 2);           // the only builtin; takes exactly one argument
print(small && !false); // prints true
// comments run to end of line
```

A variable has the type of the value it is declared with, and keeps it. Nothing
converts between the two types: `true + 1`, `!5` and assigning a bool to an int
variable are all compile errors. `true` and `false` are reserved words.

Operators, from tightest-binding to loosest:

| Operators | Operands | Result |
| --- | --- | --- |
| unary `-` `+` | int | int |
| unary `!` | bool | bool |
| `*` `/` | int, int | int |
| `+` `-` | int, int | int |
| `<` `<=` `>` `>=` | int, int | bool |
| `==` `!=` | two of the same type | bool |
| `&&` | bool, bool | bool |
| `\|\|` | bool, bool | bool |

Parentheses group. Division truncates toward zero, as in C. `&&` and `||` skip
their right-hand side when the left already settles the answer. Because a
comparison gives a bool, `3 > 2 > 1` is a type error rather than a wrong answer.

Integer literals run from `-2147483648` to `2147483647`. An expression may be
at most 500 levels deep, counting both nesting and the length of an operator
chain; past that `quarkc` reports an error instead of overflowing its stack.

Grammar:

```
program        := statement* EOF
statement      := letStmt | assignStmt | exprStmt
letStmt        := "let" IDENTIFIER "=" expression ";"
assignStmt     := IDENTIFIER "=" expression ";"
exprStmt       := expression ";"
expression     := logicalOr
logicalOr      := logicalAnd ("||" logicalAnd)*
logicalAnd     := equality ("&&" equality)*
equality       := comparison (("==" | "!=") comparison)*
comparison     := addition (("<" | "<=" | ">" | ">=") addition)*
addition       := multiplication (("+" | "-") multiplication)*
multiplication := unary (("*" | "/") unary)*
unary          := ("-" | "+" | "!") unary | primary
primary        := INTEGER | "true" | "false" | IDENTIFIER | call
                | "(" expression ")"
call           := IDENTIFIER "(" (expression ("," expression)*)? ")"
```

## How it fits together

| Stage | Files | Job |
| --- | --- | --- |
| Lexer | `Lexer.{h,cpp}`, `Token.{h,cpp}` | Source text to a token stream, tracking line and column |
| Parser | `Parser.{h,cpp}` | Recursive descent over tokens, producing the AST |
| AST | `AST.h` | Node types; a `Program` owns everything below it |
| Analyzer | `Analyzer.{h,cpp}` | Type-checks, and rejects programs that parse but cannot mean anything |
| CodeGen | `CodeGen.{h,cpp}` | AST to a self-contained C translation unit |
| Driver | `main.cpp` | CLI, stage sequencing, invoking the C compiler |

`AstView.{h,cpp}` flattens the AST into a generic labelled tree, and
`AstRender.cpp` turns that tree into text, SVG, or DOT — so the cascade that
classifies node types is written once rather than once per output format.
`Error.h` carries the position that turns an exception into a
`file.qr:3:9: error: ...` diagnostic. Lines and columns both count from 1, and
columns count characters rather than bytes.

The lexer and parser do not stop at the first mistake. The lexer marks text it
cannot read and carries on; the parser drops the statement it was in and
resumes at the next one. So one run reports every lexical and syntax error in
the file. The analyzer only runs on a program that parsed cleanly, and still
stops at its first error.

Quark variables are emitted with a `q_` prefix, so a variable named `int` or
`printf` cannot collide with anything in the generated C.

What the analyzer catches: reads of undeclared variables, duplicate
declarations, assignment before declaration, unknown functions, wrong argument
counts, `print()` used as a value, expression statements whose result is
discarded, division by a literal zero, and type errors: an operator given the
wrong type of operand, or a value assigned to a variable of the other type.

It finds the type errors by working out the type of every expression from the
types of its parts, and it leaves each answer on the AST node. The code
generator reads those to declare C variables and to print a bool as a word.

## Tests

`tests/run_tests.sh` drives the built compiler end to end.

- `tests/cases/NAME.qr` must compile, run, and print exactly `NAME.out`.
- `tests/errors/NAME.qr` must be rejected with exactly the errors in `NAME.err`,
  one per line.
- `tests/tokens/NAME.qr` must lex to exactly the token stream in `NAME.tokens`.
- `tests/emit/NAME.qr` must render to `NAME.tree` and `NAME.dot`. Its SVG is
  checked structurally — one box per AST node, document closed — rather than
  pinned to exact geometry.

Adding a test means adding the pair of files; the runner picks them up.
