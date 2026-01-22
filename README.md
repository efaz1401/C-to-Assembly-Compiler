# Tiny C-to-Assembly Compiler

This repository provides a minimal C compiler that accepts a very small subset of C and emits x86-64 assembly.
It is intended as a learning aid and not a production-ready compiler.

## Supported C subset

The compiler currently supports only a single function:

```c
int main() {
    return 2 + 3 * (4 - 1);
}
```

Supported features:

- `int main()`
- `return <expression>;`
- Integer literals
- Binary operators: `+`, `-`, `*`, `/`
- Parentheses for grouping

## Usage

Compile the compiler itself:

```bash
cc -std=c99 -Wall -Wextra -O2 compiler.c -o compiler
```

Then run it on a C input file:

```bash
./compiler examples/example.c -o example.s
```

You can assemble and link the output with `clang` or `gcc`:

```bash
clang example.s -o example
./example
```

## Notes

- The generated assembly targets x86-64 System V ABI and uses Intel syntax.
- No optimization passes are implemented.
