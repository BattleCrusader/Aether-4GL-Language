# Aether Compiler

**A fourth-generation systems language for the Aether Operating System**

This is the Aether compiler — a from-scratch language designed to build an entire operating system without a runtime, garbage collector, or interpreter. Written in C (bootstrap phase), compiling to x86_64 ELF64 binaries via NASM.

## What is Aether?

Aether is a **compiled, statically-typed, fourth-generation systems language** that bridges high-level expressiveness with bare-metal control. The compiler emits freestanding x86_64 ELF64 binaries with zero runtime dependencies.

```aether
func fib(n: u64): u64 {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}
```

### Core design principles

- **Memory management at compile time** — escape analysis, region inference, no GC
- **No interpreter, no runtime** — standalone freestanding binaries
- **NASM is a first-class citizen** — full inline assembly with variable binding
- **Classes optional, automatic destruction** — zero-cost abstractions
- **References over pointers** — `ref T`, `owned T`, `rc T`, `ptr T` for raw work

## Status

| Phase | Status |
|-------|--------|
| Phase 0 — Bootstrap Toolchain | 🟢 Complete |
| Phase 1 — Core Language | 🔵 In Progress |
| Phase 2 — Memory Management | 🔴 Not Started |
| Phase 3 — OOP and Type System | 🔴 Not Started |
| Phase 4 — Advanced Features | 🔴 Not Started |
| Phase 5 — Aether OS Integration | 🔴 Not Started |
| Phase 6 — Self-Hosting | 🔴 Not Started |
| Phase 7 — Optimization & Polish | 🔴 Not Started |

## Quick Start

```bash
# Build the bootstrap compiler
make

# Compile a .ae file to an ELF64 binary
./build/aether hello.ae -o hello.elf

# Stop at assembly (inspect the .asm)
./build/aether hello.ae -S

# Dump AST for debugging
./build/aether hello.ae --dump-ast

# Dump token stream
./build/aether hello.ae --dump-tokens
```

## Prerequisites

- **nasm** — Netwide Assembler (for assembling generated .asm to .o)
- **x86_64-elf-ld** — ELF64 cross-linker
- **gcc** or **clang** — for building the bootstrap compiler

## Project Structure

```
compiler/
├── src/           # C source files (bootstrap compiler)
│   ├── tokenizer.c   # Lexical analysis
│   ├── parser.c      # Recursive descent + Pratt parser
│   ├── ast.c         # AST node definitions and dump
│   ├── semantic.c    # Name resolution, scope, type checking
│   ├── codegen.c     # NASM assembly generation
│   ├── lexer.c       # Token stream with lookahead
│   ├── arena.c       # Arena allocator for AST/memory
│   ├── vector.c      # Dynamic arrays
│   ├── str.c         # String view utilities
│   └── aether.c      # CLI entry point
├── include/aether/  # Header files
├── tests/           # Test suite and fixtures
├── REQUIREMENTS.md  # Language and compiler requirements
├── SPECIFICATION.md # Full language specification with code examples
├── STATUS.md        # Detailed implementation status
└── PHASE_XX.md      # Phase-specific task breakdowns
```

## Why Aether?

Existing languages are excellent at what they do, but none combine **automatic memory management**, **zero runtime dependencies**, **compiled-to-bare-metal output**, and **OS-native syscall integration** in a single package. Aether is designed to build the Aether OS—a from-scratch x86_64 operating system—entirely within its own toolchain.

## Credits

This compiler and language are being written by **Hermes Agent (by Nous Research)** — a persistent autonomous AI agent operating under the direction of the Aether OS project. Every line of C, every language design decision, and every specification document has been authored by Hermes as part of an ongoing session to build a complete systems programming language from scratch.

---

*Phase 1 delivery: Core language compiler with stack frame tracking, type-aware loads/stores (u8/u16/u32/u64), struct allocation and field access, string literal table in `.rodata`, proper if/while/for/match codegen, and SysV argument passing. Test programs compile to correct x86_64 ELF64 binaries via `func name(params: type): return_type { }` syntax.*