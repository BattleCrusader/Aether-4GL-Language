# Aether Compiler — AGENTS.md

> **Primary entry point for AI agents (Claude Code, Codex, Cursor, Copilot, etc.)**
> Read this first before making any changes. This file is kept up to date with the actual state of the codebase.

---

## Quick Facts

- **Language**: C11 (bootstrap), targeting Aether (self-hosting goal)
- **Output**: LLVM IR → native code via LLVM backends (ELF64/Mach-O/PE32+/flat binary)
- **Build**: `make` → `./build/aether`
- **Test**: `make test` (unit) + `make test-host` (native .ae fixtures)
- **Install**: `sudo make install` or `make install-local`
- **Source**: `/Volumes/Backup/Development/Project_Aether/compiler/`
- **Branch**: `feature/P37.00-debug-release-contracts` (active development)

---

## Project Structure

```
compiler/
├── src/                    # C source files (bootstrap compiler)
│   ├── aether.c            # CLI entry point, import resolution, pipeline orchestration
│   ├── tokenizer.c         # Keyword table, token type names, tokenization
│   ├── lexer.c             # Lexer stream (indentation engine, token advancement)
│   ├── ast.c               # AST node creation helpers
│   ├── parser.c            # Recursive descent + Pratt parser
│   ├── semantic.c          # Type checking, name resolution, const evaluation
│   ├── optimizer.c         # DCE, constant folding, inlining, escape analysis
│   ├── llvm/               # LLVM IR backend (13 modules, ~2350 lines total)
│   │   ├── llvm_init.c     # LLVM context/module/builder creation
│   │   ├── llvm_types.c    # Aether → LLVM type mapping
│   │   ├── llvm_expr.c     # Expression codegen (literals, idents, ops, calls, indexing)
│   │   ├── llvm_stmt.c     # Statement codegen (let, if, while, for, return, defer, block)
│   │   ├── llvm_func.c     # Function codegen (decl, params, entry/exit, variadics)
│   │   ├── llvm_string.c   # String operations (concat, interpolation, itoa)
│   │   ├── llvm_asm.c      # Inline assembly (asm blocks, output bindings)
│   │   ├── llvm_error.c    # Error handling (throws, try/catch, cleanup tables)
│   │   ├── llvm_contract.c # Contract codegen (pre/post conditions)
│   │   ├── llvm_runtime.c  # Runtime helpers (alloc, concat, itoa declarations)
│   │   ├── llvm_target.c  # Target setup, object emission, linker scripts
│   │   └── llvm_debug.c    # Debug info (DWARF metadata, source locations)
│   ├── arena.c             # Arena allocator
│   ├── str.c               # String view utilities
│   └── vector.c            # Dynamic array
├── include/aether/         # Header files
│   ├── defs.h              # Common definitions, StringView, AstNodeList
│   ├── ast.h               # All AST node types (50+), binary/unary ops
│   ├── tokenizer.h         # Token types, keyword table
│   ├── lexer.h             # Lexer state
│   ├── parser.h            # Parser state, precedence levels
│   ├── semantic.h          # Semantic analyzer
│   ├── llvm.h              # LlvmCodegen state, public API for all LLVM modules
│   ├── optimizer.h         # Optimizer config
│   ├── arena.h             # Arena allocator
│   ├── str.h               # String view
│   └── vector.h            # Dynamic array
├── tests/                  # C test suite + .ae fixture programs
│   ├── test_tokenizer.c    # Tokenizer unit tests
│   ├── test_parser.c       # Parser unit tests
│   ├── test_llvm_types.c   # LLVM type mapping tests
│   ├── test_llvm_expr.c    # LLVM expression codegen tests
│   ├── test_llvm_stmt.c    # LLVM statement codegen tests
│   ├── test_llvm_func.c    # LLVM function codegen tests
│   ├── test_llvm_string.c  # LLVM string operation tests
│   ├── test_llvm_asm.c     # LLVM inline assembly tests
│   ├── test_llvm_error.c   # LLVM error handling tests
│   ├── test_llvm_contract.c# LLVM contract codegen tests
│   ├── test_llvm_target.c  # LLVM target emission tests
│   ├── fixtures/           # .ae test programs
│   └── debug_*.c           # Debug utilities
├── std/                    # Standard library (11 .ae modules)
│   ├── io.ae               # print, println, format, read_line
│   ├── mem.ae              # alloc, free, copy, zero, Pool, Arena
│   ├── str.ae              # String, concat, split, trim
│   ├── math.ae             # sqrt, sin, cos, abs, min, max
│   ├── collections.ae      # Array, HashMap, Set, List, Queue
│   ├── fs.ae               # File, Path, Directory (AetherFS syscalls)
│   ├── serial.ae           # COM1 serial I/O (kernel mode)
│   ├── elf.ae              # ELF64 reader/writer
│   ├── test.ae             # assert, test_runner, benchmark
│   ├── asm.ae              # NASM helper macros
│   └── arch.ae             # Architecture detection, multi-target helpers
├── REQUIREMENTS.md         # Comprehensive language requirements
├── SPECIFICATION.md        # Full language specification with code examples
├── STATUS.md               # Implementation status (20 phases)
├── LLVM_BACKEND.md         # LLVM backend architecture & design
├── AGENTS.md               # THIS FILE — AI agent guide
├── CONTRIBUTING.md         # Human contributor guide
├── Makefile                # Build system
├── LICENSE                 # License file
└── README.md               # Project overview
```

---

## Compiler Pipeline

```
Source (.ae)
  → Tokenizer (whitespace-aware, indent engine)                    ✅
    → Parser (Pratt + recursive descent)                           ✅
      → AST (50+ node types)                                       ✅
        → Import Resolution (read, parse, merge imported files)     ✅
          → Semantic Analysis (type checking, name resolution)     ✅
            → LLVM Codegen (LLVM IR via LLVM-C API)                 ✅
              → LLVM Optimization (200+ passes)                    ✅
                → LLVM Backend (x86_64/ARM64/RISC-V/WASM)          ✅
                  → Binary Output (ELF64/Mach-O/PE32+/flat binary) ✅
```

---

## Key Source Files — What Each Does

### `src/aether.c` — CLI & Pipeline Orchestration
- Entry point: `main()` parses CLI args, dispatches to `aether_init`, `aether_build`, `aether_run`
- Pipeline: `aether_compile()` calls tokenize → parse → import_resolve → semantic → optimize → codegen → assemble
- Import resolution: `resolve_imports()` reads imported files, parses with shared arena, merges declarations
- CLI flags: `--target`, `-o`, `-L`, `-S`, `--dump-ast`, `--dump-tokens`, `-v`
- `aether_init()` scaffolds new projects
- `aether_run()` compiles + executes in one step

### `src/tokenizer.c` — Keyword Table & Tokenization
- `KEYWORDS[]` table: 75+ keywords mapped to `TokenType` enums
- `token_type_name()`: maps every token type to human-readable string
- `tokenize()`: main tokenization function
- Keywords include: func, let, mut, if, elif, else, while, for, in, return, struct, enum, class, match, try, throw, catch, import, const, ref, owned, rc, heap, region, defer, unsafe, module, sys, trait, impl, pool, protocol, dyn, throws, export, entry, layout, prop, inline, iflet, and more

### `src/lexer.c` — Lexer Stream
- `lexer_create()`: initializes lexer with source text
- `lexer_advance()`: advances to next token (handles indentation)
- Indentation engine: tracks indent levels, emits INDENT/DEDENT tokens
- Token management: `Token` struct with type, text (StringView), line, column

### `src/ast.c` — AST Node Creation
- `ast_create_*()`: factory functions for each node type
- All nodes arena-allocated (no individual frees)
- `AstNode` union type with 50+ variants

### `src/parser.c` — Recursive Descent + Pratt Parser
- `parser_create()` / `parser_create_with_arena()`: parser initialization
- `parser_parse()`: parses entire source into NODE_PROGRAM
- `parse_declaration()`: top-level dispatch (func, struct, enum, class, const, import, module, trait, impl, asm)
- `parse_func_decl()`: function declarations with params, return types, default params
- `parse_struct_decl()` / `parse_enum_decl()` / `parse_class_decl()`: type declarations
- `parse_statement()`: let, if, while, for, match, try, throw, defer, return, break, continue, asm, region
- `parse_expr_prec()`: Pratt parser with precedence climbing (14 levels)
- `parse_block()`: indentation-sensitive block parsing
- `parse_type()`: type expressions (primitives, arrays, slices, pointers, references, optionals, tuples, function types)
- String interpolation: `parse_string_interp()` scans for `{expr}` and builds BIN_CONCAT chains
- `parse_asm_block()`: captures raw NASM text between `asm { }`

### `src/semantic.c` — Type Checking & Name Resolution
- Two-pass analysis: first pass declares all names, second pass visits bodies
- `const_eval_expr()`: compile-time constant evaluation (arithmetic, bitwise, logical, sizeof, alignof)
- `scope_lookup()` / `scope_declare()`: symbol table management
- `semantic_analyze()`: main entry point, walks AST and resolves types
- `resolve_types()`: type resolution and checking

### `src/llvm/` — LLVM IR Backend (13 modules)

See [LLVM_BACKEND.md](LLVM_BACKEND.md) for full design documentation.

| Module | Lines | Purpose |
|--------|-------|---------|
| `llvm_init.c` | 80 | LLVM context/module/builder creation |
| `llvm_types.c` | 120 | Aether → LLVM type mapping |
| `llvm_expr.c` | 350 | Expression codegen (12 functions) |
| `llvm_stmt.c` | 300 | Statement codegen (12 functions) |
| `llvm_func.c` | 250 | Function codegen |
| `llvm_string.c` | 200 | String operations |
| `llvm_asm.c` | 150 | Inline assembly |
| `llvm_error.c` | 200 | Error handling |
| `llvm_contract.c` | 100 | Contract codegen |
| `llvm_runtime.c` | 150 | Runtime helpers |
| `llvm_target.c` | 200 | Target setup & emission |
| `llvm_debug.c` | 150 | Debug info |

### `src/optimizer.c` — Optimization Passes
- `optimize()`: runs all enabled passes in order
- `opt_constant_fold()`: constant folding and propagation
- `opt_dead_code_elim()`: DCE (two-pass for cross-file references)
- `opt_inline()`: function inlining (heuristic + @force_inline)
- `opt_escape_analysis()`: heap/stack promotion (placeholder)
- `opt_region_elision()`: arena elision (placeholder)
- `opt_devirtualize()`: dynamic→static dispatch conversion (placeholder)
- `opt_loop_unroll()`: loop unrolling (placeholder)
- `opt_memory_fusion()`: memory operation fusion (placeholder)

---

## Compiler Targets

| Target | Format | Entry Point | Use Case |
|--------|--------|-------------|----------|
| `host` | Mach-O/ELF64 | `_aether_entry` | Dev machine testing |
| `x86_64-freestanding` | ELF64 | `_start` | Aether OS kernel |
| `kernel` | ELF64 | `_start` at 0x1000000 | Kernel with memory map verification |
| `module` | ELF64 `.ko` | `mod_init` | Loadable kernel module |
| `binary` | ELF64 | `main` at 0x2000000 | Userland binary |
| `boot` | Flat binary | `start` at 0x7C00 | Boot sector |
| `asm-x86_64` | Intel assembly | — | Assembly listing |
| `asm-arm64` | ARM64 assembly | — | ARM64 listing |
| `asm-riscv64` | RISC-V assembly | — | RISC-V listing |
| `universal` | Multi-arch ELF | CPU detection | x86_64 + ARM64 |
| `universal-all` | Multi-arch ELF | CPU detection | All architectures |
| `wasm32` | WebAssembly | — | Web/embedded (future) |

---

## Runtime Helpers (auto-emitted by codegen)

| Function | Purpose | Notes |
|----------|---------|-------|
| `__aether_alloc(size: u64)` | Bump allocator | mmap-backed on host, no-op on kernel |
| `__aether_free(ptr)` | No-op free | Bump allocator doesn't free individually |
| `__aether_concat(left, right)` | String concatenation | Allocates + copies both strings. Handles null pointers (treats as empty string). |
| `__aether_itoa(value: u64)` | u64 to decimal string | Allocates 21 bytes |

---

## Known Technical Decisions & Pitfalls

### Critical — Read Before Making Changes

1. **`+` operator does string concat when either operand is a string**: Detected at codegen time by `is_string_expr()`. Numeric addition when both are numbers. This is NOT a parser-level decision — it's a codegen-level decision.

2. **DCE must handle NODE_EXPR_STMT**: The DCE optimizer must keep `let` statements with function call, asm block, or binary op initializers even if the variable is unused. Two-pass DCE for cross-file references.

3. **Kernel must be built with `-O0`**: The optimizer can remove side-effectful calls. Use `-O0` for kernel builds.

4. **Import resolution keeps source buffers alive**: Imported AST nodes' StringView fields point into the imported source buffer. The buffer must be kept alive for the entire compilation. Circular imports are detected and skipped.

5. **Two-pass semantic analysis**: First pass declares all top-level names. Second pass visits function bodies. This handles forward references across files.

6. **String interpolation builds BIN_CONCAT chains**: `"Hello {name}!"` becomes `BIN_CONCAT("Hello ", BIN_CONCAT(name, "!"))`. Numeric expressions are auto-converted via `__aether_itoa`.

7. **`__aether_concat` and `print()` must handle null string pointers**: When `main(inputString: string)` receives no args, the string pointer is null. The concat helper and print built-in must test for null before dereferencing. Null is treated as an empty string (len=0, no copy).

8. **LLVM backend replaces NASM codegen**: The old `src/codegen.c` (NASM text generation) and `src/asm_*.c` (multi-target assembler) are superseded by the LLVM backend in `src/llvm/`. Do not add new features to the NASM codegen — add them to the LLVM backend instead.

---

## How to Contribute

### Adding a New Language Feature

1. **Tokenizer**: Add new keyword/token type in `tokenizer.c` and `tokenizer.h`
2. **AST**: Add new node type in `ast.h` and create helper in `ast.c`
3. **Parser**: Add parsing logic in `parser.c`
4. **Semantic**: Add type checking in `semantic.c`
5. **Codegen**: Add code emission in the appropriate `src/llvm/*.c` module
6. **Optimizer**: Update DCE/optimization passes in `optimizer.c`
7. **Tests**: Add test fixture in `tests/fixtures/` and update `Makefile`
8. **Docs**: Update REQUIREMENTS.md, SPECIFICATION.md, STATUS.md, AGENTS.md

### Adding a New Compiler Target

1. Add target enum in `include/aether/llvm.h` (Target enum)
2. Add target triple in `src/llvm/llvm_target.c` (`llvm_target_triple()`)
3. Add entry point / syscall handling in `src/llvm/llvm_func.c`
4. Add emission logic in `src/llvm/llvm_target.c`
5. Add CLI flag in `aether.c` (`parse_args()`)
6. Add test fixture and update Makefile

### Running Tests

```bash
make test           # Unit tests (tokenizer + parser + LLVM modules)
make test-host      # Native .ae fixture tests
make test-layout    # Flat binary layout tests
```

### Build & Install

```bash
make                # Build bootstrap compiler
sudo make install   # Install to /usr/local
make install-local  # Install to ~/.local
```

---

## Implementation Status (Summary)

| Phase | Status | Description |
|-------|--------|-------------|
| 0-6 | 🟢 COMPLETE | Bootstrap, core language, host-native, memory mgmt, OOP, advanced features, OS integration |
| 7 | 🔴 NOT STARTED | Self-hosting |
| 8-11 | 🟢 COMPLETE | Multi-target assembler, optimization, universal binaries, kernel codegen, @layout |
| 12 | 🟢 COMPLETE | Language specification & requirements |
| 13 | 🔴 NOT STARTED | Concurrency & fibers |
| 14 | 🔴 NOT STARTED | Advanced OS language features |
| 15 | 🔴 NOT STARTED | Goal-oriented I/O & query fusion |
| 16 | 🔴 NOT STARTED | Protocol generation & hardware configuration |
| 17 | 🟢 COMPLETE | `.aelib` library format |
| 18 | 🟢 COMPLETE | Standard library implementation |
| 19 | 🔴 NOT STARTED | LLVM backend migration |
| 20 | 🔴 NOT STARTED | Self-hosting |

See [STATUS.md](STATUS.md) for detailed per-phase checklists.

---

## Key Files Reference

| File | Lines | Purpose |
|------|-------|---------|
| `src/parser.c` | 1861 | Recursive descent parser |
| `src/aether.c` | 1454 | CLI entry point, pipeline orchestration |
| `src/optimizer.c` | 960 | Optimization passes |
| `src/tokenizer.c` | 690 | Keyword table, tokenization |
| `src/semantic.c` | 700 | Type checking, name resolution |
| `src/ast.c` | 590 | AST node creation |
| `include/aether/ast.h` | 560 | AST node types (50+) |
| `src/llvm/` (13 files) | ~2350 | LLVM IR backend |
| `REQUIREMENTS.md` | 755 | Language requirements |
| `SPECIFICATION.md` | 2773 | Language specification |
| `STATUS.md` | 540 | Implementation status |
| `LLVM_BACKEND.md` | 780 | LLVM backend design |
