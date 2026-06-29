# Aether Compiler

**A fourth-generation systems language for the Aether Operating System**

This is the Aether compiler — a from-scratch language designed to build an entire operating system without a runtime, garbage collector, or interpreter. Written in C (bootstrap phase), compiling through LLVM IR to native code, with host-native output for macOS/Linux development and freestanding output for kernel/boot targets.

```aether
func fib(n: u64): u64 {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}
```

### Core design principles

- **Memory management at compile time** — escape analysis, region inference, defer/scope cleanup, no GC
- **No interpreter, no runtime** — standalone freestanding binaries, runs from the first byte
- **LLVM IR backend** — register allocation, optimization, and multi-arch support handled by LLVM
- **Inline assembly** — full Intel-syntax inline assembly via LLVM, with variable binding and output constraints
- **Classes optional, automatic destruction** — zero-cost abstractions, auto-init/drop at scope exit
- **References over pointers** — `ref T`, `owned T`, `rc T`, with raw `ptr T` for low-level work
- **Host-native development** — compile and run `.ae` programs directly on macOS/Linux without a VM

```
aether build hello.ae --target host && ./hello
# or
aether run hello.ae
```

## Status

Full phase breakdown in [STATUS.md](STATUS.md). Detailed requirements in [REQUIREMENTS.md](REQUIREMENTS.md). Language specification in [SPECIFICATION.md](SPECIFICATION.md). LLVM backend design in [LLVM_BACKEND.md](LLVM_BACKEND.md).

## Quick Start

```bash
# Build the bootstrap compiler
make

# Compile and run natively (macOS/Linux)
./build/aether --target host hello.ae -o hello
./hello

# Compile for Aether OS (freestanding ELF64)
./build/aether --target x86_64-freestanding hello.ae -o hello.elf

# One-step compile+execute
./build/aether run hello.ae

# Stop at assembly (inspect the .asm)
./build/aether hello.ae -S

# Dump AST or tokens
./build/aether hello.ae --dump-ast
./build/aether hello.ae --dump-tokens

# Run test suites
make test           # Unit tests (tokenizer + parser)
make test-host      # Native compilation + execution tests
```

## Prerequisites

- **LLVM 18+** — `brew install llvm` (macOS) or `apt install llvm-dev` (Linux)
- **gcc** or **clang** — for building the bootstrap compiler
- **make** — build system

## Project Structure

```
compiler/
├── src/                 # C source files (bootstrap compiler)
│   ├── aether.c         # CLI entry point, pipeline orchestration
│   ├── tokenizer.c      # Keyword table, tokenization
│   ├── lexer.c          # Lexer stream, indentation engine
│   ├── ast.c            # AST node creation helpers
│   ├── parser.c         # Recursive descent + Pratt parser
│   ├── semantic.c       # Type checking, name resolution
│   ├── optimizer.c       # DCE, constant folding, inlining
│   ├── llvm/            # LLVM IR backend (13 modules)
│   │   ├── llvm_init.c      # LLVM context/module/builder
│   │   ├── llvm_types.c     # Aether → LLVM type mapping
│   │   ├── llvm_expr.c      # Expression codegen
│   │   ├── llvm_stmt.c      # Statement codegen
│   │   ├── llvm_func.c      # Function codegen
│   │   ├── llvm_string.c    # String operations
│   │   ├── llvm_asm.c       # Inline assembly
│   │   ├── llvm_error.c     # Error handling
│   │   ├── llvm_contract.c  # Contract codegen
│   │   ├── llvm_runtime.c   # Runtime helpers
│   │   ├── llvm_target.c    # Target setup & emission
│   │   └── llvm_debug.c     # Debug info
│   ├── arena.c          # Arena allocator
│   ├── str.c            # String view utilities
│   └── vector.c         # Dynamic array
├── include/aether/      # Header files
├── tests/               # C test suite + .ae fixture programs
├── std/                 # Standard library (11 .ae modules)
├── REQUIREMENTS.md      # Language and compiler requirements
├── SPECIFICATION.md     # Full language specification with examples
├── STATUS.md            # Detailed implementation status per phase
├── LLVM_BACKEND.md      # LLVM backend architecture & design
├── AGENTS.md            # AI agent guide
├── CONTRIBUTING.md      # Human contributor guide
└── Makefile             # Build system
```

## Credits

Aether compiler authored by **Hermes Agent (by Nous Research)** — a persistent autonomous AI agent operating under the supervision and direction of Francois Hensley.
