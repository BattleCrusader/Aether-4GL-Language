# Aether Compiler — Contributing Guide

> **For human contributors.** If you're an AI agent, read [AGENTS.md](AGENTS.md) instead.

---

## Welcome

Thank you for your interest in contributing to the Aether compiler! This is a bootstrap compiler written in C11 that compiles the Aether 4GL language. The end goal is self-hosting — the compiler will eventually compile itself.

## Quick Start

```bash
# Clone the repository
cd /Volumes/Backup/Development/Project_Aether/compiler

# Build the bootstrap compiler
make

# Run the test suite
make test           # Unit tests (tokenizer + parser)
make test-host      # Native .ae fixture tests

# Compile and run a test program
./build/aether --target host tests/fixtures/hello.ae -o /tmp/hello
/tmp/hello
echo $?             # Should print 42

# One-step compile+execute
./build/aether run tests/fixtures/hello.ae
```

## Prerequisites

- **C compiler**: GCC or Clang (for building the bootstrap compiler)
- **LLVM 18+**: LLVM development libraries (for the LLVM IR backend)
- **make**: Build system
- **Python 3**: For some build tools

On macOS:
```bash
brew install llvm
```

On Linux:
```bash
sudo apt-get install llvm-dev
```

## Project Overview

The Aether compiler is a from-scratch language compiler that:

1. Reads `.ae` source files
2. Tokenizes and parses them into an AST
3. Performs semantic analysis and optimization
4. Generates LLVM IR via the LLVM-C API
5. Emits object files, assembly listings, or flat binaries via LLVM backends

The compiler is written in C11 (bootstrap phase) and will eventually be rewritten in Aether (self-hosting).

## Codebase Tour

### Source Files (`src/`)

| File | What it does | When to touch it |
|------|-------------|------------------|
| `aether.c` | CLI entry point, pipeline orchestration, import resolution | Adding CLI flags, changing build pipeline |
| `tokenizer.c` | Keyword table, tokenization | Adding new keywords or token types |
| `lexer.c` | Lexer stream, indentation engine | Changing indentation rules |
| `ast.c` | AST node creation helpers | Adding new AST node types |
| `parser.c` | Recursive descent + Pratt parser | Adding new syntax, changing grammar |
| `semantic.c` | Type checking, name resolution, const evaluation | Adding type rules, compile-time evaluation |
| `optimizer.c` | DCE, constant folding, inlining | Adding optimization passes, fixing optimizer bugs |
| `llvm/llvm_init.c` | LLVM context/module/builder creation | Changing LLVM initialization |
| `llvm/llvm_types.c` | Aether → LLVM type mapping | Adding new primitive types |
| `llvm/llvm_expr.c` | Expression codegen (literals, ops, calls, indexing) | Adding expression-level features |
| `llvm/llvm_stmt.c` | Statement codegen (let, if, while, for, return, defer) | Adding statement-level features |
| `llvm/llvm_func.c` | Function codegen (decl, params, entry/exit, variadics) | Changing function ABI |
| `llvm/llvm_string.c` | String operations (concat, interpolation, itoa) | Changing string representation |
| `llvm/llvm_asm.c` | Inline assembly (asm blocks, output bindings) | Changing inline asm support |
| `llvm/llvm_error.c` | Error handling (throws, try/catch, cleanup tables) | Changing exception mechanism |
| `llvm/llvm_contract.c` | Contract codegen (pre/post conditions) | Changing contract semantics |
| `llvm/llvm_runtime.c` | Runtime helpers (alloc, concat, itoa declarations) | Adding runtime functions |
| `llvm/llvm_target.c` | Target setup, object emission, linker scripts | Adding new targets |
| `llvm/llvm_debug.c` | Debug info (DWARF metadata, source locations) | Improving debug experience |

### Header Files (`include/aether/`)

| File | Key types/defines |
|------|-------------------|
| `defs.h` | `StringView`, `AstNodeList`, common macros |
| `ast.h` | `NodeType` enum (50+ types), `AstNode` union, `BinOp`/`UnaryOp` enums |
| `tokenizer.h` | `TokenType` enum, `Token` struct, `KeywordEntry` |
| `lexer.h` | `Lexer` struct |
| `parser.h` | `Parser` struct, `Precedence` enum |
| `semantic.h` | `SemanticAnalyzer` struct |
| `llvm.h` | `LlvmCodegen` struct, public API for all LLVM modules |
| `optimizer.h` | `OptimizerConfig` struct |

### Standard Library (`std/`)

The standard library is written in Aether (`.ae`) and provides:

- `io.ae` — print, println, format, read_line
- `mem.ae` — alloc, free, copy, zero, Pool, Arena
- `str.ae` — String, concat, split, trim
- `math.ae` — sqrt, sin, cos, abs, min, max
- `collections.ae` — Array, HashMap, Set, List, Queue
- `fs.ae` — File, Path, Directory (AetherFS syscalls)
- `serial.ae` — COM1 serial I/O (kernel mode)
- `elf.ae` — ELF64 reader/writer
- `test.ae` — assert, test_runner, benchmark
- `asm.ae` — NASM helper macros
- `arch.ae` — Architecture detection, multi-target helpers

## How to Contribute

### Reporting Bugs

Open an issue with:
- The `.ae` source that triggers the bug
- Expected behavior
- Actual behavior (compiler error, wrong output, crash)
- Compiler version (`./build/aether --version`)

### Adding a New Language Feature

Follow this order:

1. **Tokenizer** (`tokenizer.c` + `tokenizer.h`):
   - Add new keyword to `KEYWORDS[]` table
   - Add new `TokenType` to `tokenizer.h`
   - Add token name to `token_type_name()`

2. **AST** (`ast.h` + `ast.c`):
   - Add new `NodeType` enum value
   - Add union member to `AstNode`
   - Add factory function in `ast.c`

3. **Parser** (`parser.c` + `parser.h`):
   - Add parsing function
   - Wire into `parse_declaration()` or `parse_statement()` or `parse_expr_prec()`

4. **Semantic** (`semantic.c` + `semantic.h`):
   - Add type checking rules
   - Update `const_eval_expr()` if compile-time evaluable

5. **Codegen** (in `src/llvm/`):
   - Add code emission in the appropriate LLVM module
   - `llvm_expr.c` for expression features
   - `llvm_stmt.c` for statement features
   - `llvm_func.c` for function-level features
   - `llvm_string.c` for string operations
   - `llvm_error.c` for error handling
   - `llvm_asm.c` for inline assembly
   - `llvm_contract.c` for contract programming

6. **Optimizer** (`optimizer.c` + `optimizer.h`):
   - Update DCE to handle new node types
   - Add optimization pass if applicable

7. **Tests**:
   - Add `.ae` test fixture in `tests/fixtures/`
   - Add expected exit code to `TEST_EXPECTED` in `Makefile`
   - Add fixture to `TEST_FIXTURES` in `Makefile`
   - Run `make test-host` to verify

8. **Documentation**:
   - Update `REQUIREMENTS.md` if adding a new feature area
   - Update `SPECIFICATION.md` with syntax and examples
   - Update `STATUS.md` with new phase items
   - Update `AGENTS.md` with new source file info

### Fixing a Bug

1. Reproduce the bug with a minimal `.ae` test case
2. Identify which pipeline stage is at fault (tokenizer? parser? semantic? codegen?)
3. Fix in the appropriate source file
4. Add the test case to the test suite
5. Run `make test && make test-host` to verify no regressions
6. Update `STATUS.md` if applicable

### Code Style

- **C source**: C11 standard, freestanding-compatible
- **Indentation**: 4 spaces (no tabs)
- **Naming**: `snake_case` for functions and variables, `PascalCase` for types
- **Comments**: `/* Block comments */` for C, `//` for Aether
- **Error handling**: Return error codes, use `fprintf(stderr, ...)` for errors
- **No dynamic allocation**: Use arena allocator for AST nodes
- **No stdlib**: The compiler must compile with `-ffreestanding` eventually
- **LLVM modules**: Each file under 400 lines, one concern per file

### Testing

```bash
# Run all tests
make test           # Unit tests (tokenizer + parser)
make test-host      # Native .ae fixture tests
make test-layout    # Flat binary layout tests

# Run specific test
./build/aether --target host tests/fixtures/test_math.ae -o /tmp/test && /tmp/test
echo $?             # Should match expected exit code
```

### Commit Messages

Use descriptive multi-line commit messages:

```
component: Brief description of change

Detailed explanation of what was changed and why.
Include any relevant context, trade-offs, or alternatives considered.

Fixes #123
```

## Architecture Decisions

### Why C11?
- Matches the Aether OS kernel's freestanding requirements
- Simple, portable, well-understood
- Easy to bootstrap (every platform has a C compiler)

### Why LLVM?
- Register allocation, instruction selection, and optimization handled by LLVM
- Free multi-arch backends: x86_64, ARM64, RISC-V, WASM
- Proper DWARF debug info
- Battle-tested optimization passes (200+)
- The LLVM-C API is stable and well-documented

### Why Arena Allocation?
- No individual frees needed (entire arena freed at once)
- Predictable performance (O(1) allocation)
- No fragmentation
- Perfect for compiler ASTs (all nodes freed together)

### Why Python-style Indentation?
- Readable and self-documenting
- Reduces syntactic noise (no braces)
- Consistent with the language's readability-first philosophy

## Getting Help

- Read [REQUIREMENTS.md](REQUIREMENTS.md) for language requirements
- Read [SPECIFICATION.md](SPECIFICATION.md) for language specification
- Read [STATUS.md](STATUS.md) for implementation status
- Read [LLVM_BACKEND.md](LLVM_BACKEND.md) for LLVM backend design
- Read [AGENTS.md](AGENTS.md) for AI agent guide (also useful for humans)
- Read the source code — it's well-commented
