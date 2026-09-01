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

One type (32-bit `int`), one scope (global), one builtin (`print`).

```
let x = 10;        // declare
x = x + 1;         // assign to something already declared
print(x * 2);      // the only builtin; takes exactly one argument
// comments run to end of line
```

Operators are `+ - * /` with the usual precedence, unary `-` and `+`, and
parentheses for grouping. Division truncates toward zero, as in C.

Grammar:

```
program        := statement* EOF
statement      := letStmt | assignStmt | exprStmt
letStmt        := "let" IDENTIFIER "=" expression ";"
assignStmt     := IDENTIFIER "=" expression ";"
exprStmt       := expression ";"
expression     := addition
addition       := multiplication (("+" | "-") multiplication)*
multiplication := unary (("*" | "/") unary)*
unary          := ("-" | "+") unary | primary
primary        := INTEGER | IDENTIFIER | call | "(" expression ")"
call           := IDENTIFIER "(" (expression ("," expression)*)? ")"
```

## How it fits together

| Stage | Files | Job |
| --- | --- | --- |
| Lexer | `Lexer.{h,cpp}`, `Token.{h,cpp}` | Source text to a token stream, tracking line numbers |
| Parser | `Parser.{h,cpp}` | Recursive descent over tokens, producing the AST |
| AST | `AST.h` | Node types; a `Program` owns everything below it |
| Analyzer | `Analyzer.{h,cpp}` | Rejects programs that parse but cannot mean anything |
| CodeGen | `CodeGen.{h,cpp}` | AST to a self-contained C translation unit |
| Driver | `main.cpp` | CLI, stage sequencing, invoking the C compiler |

`AstView.{h,cpp}` flattens the AST into a generic labelled tree, and
`AstRender.cpp` turns that tree into text, SVG, or DOT — so the cascade that
classifies node types is written once rather than once per output format.
`Error.h` carries the line number that turns an exception into a
`file.qr:3: error: ...` diagnostic.

Quark variables are emitted with a `q_` prefix, so a variable named `int` or
`printf` cannot collide with anything in the generated C.

What the analyzer catches: reads of undeclared variables, duplicate
declarations, assignment before declaration, unknown functions, wrong argument
counts, expression statements whose result is discarded, and division by a
literal zero.

## Tests

`tests/run_tests.sh` drives the built compiler end to end.

- `tests/cases/NAME.qr` must compile, run, and print exactly `NAME.out`.
- `tests/errors/NAME.qr` must be rejected with the message in `NAME.err`.
- `tests/emit/NAME.qr` must render to `NAME.tree` and `NAME.dot`. Its SVG is
  checked structurally — one box per AST node, document closed — rather than
  pinned to exact geometry.

Adding a test means adding the pair of files; the runner picks them up.
