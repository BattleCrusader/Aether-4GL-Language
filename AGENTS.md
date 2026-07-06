# Aether Compiler — AGENTS.md

> **Primary entry point for AI agents (Claude Code, Codex, Cursor, Copilot, etc.)**
> Read this first before making any changes. This file is kept up to date with the actual state of the codebase.

---

## Quick Facts

- **Language**: C11 (bootstrap), targeting Aether (self-hosting goal)
- **Output**: C transpiler → native code via gcc/clang (ELF64/Mach-O/PE32+/flat binary)
- **Build**: `make` → `./build/aether`
- **Test**: `make test` (unit) + `make test-host` (native .ae fixtures)
- **Install**: `sudo make install` or `make install-local`
- **Source**: `/Volumes/Backup/Development/Project_Aether/compiler/`
- **Branch**: `feature/P40.00-error-context-operator` (active development)

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
│   ├── codegen/            # NASM codegen modules (15 files, ~200KB total)
│   │   ├── codegen_init.c  # Codegen create/destroy
│   │   ├── codegen_expr.c  # Expression codegen
│   │   ├── codegen_stmt.c  # Statement codegen
│   │   ├── codegen_func.c  # Function codegen
│   │   ├── codegen_top.c   # Top-level codegen
│   │   ├── codegen_target.c # Target setup
│   │   ├── codegen_output.c # Output helpers
│   │   ├── codegen_frame.c # Stack frame
│   │   ├── codegen_defer.c # Defer
│   │   ├── codegen_aelib.c # Aelib metadata
│   │   ├── codegen_assemble.c # Assemble/link
│   │   ├── codegen_enum_layout.c # Enum layout
│   │   ├── codegen_type_helpers.c # Type helpers
│   │   ├── codegen_mem_map.c # Memory map
│   │   └── codegen_internal.h # Internal header
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
│   ├── codegen.h           # NASM codegen state, public API
│   ├── c_transpiler.h      # C transpiler state, public API
│   ├── optimizer.h         # Optimizer config
│   ├── arena.h             # Arena allocator
│   ├── str.h               # String view
│   └── vector.h            # Dynamic array
├── tests/                  # C test suite + .ae fixture programs
│   ├── test_tokenizer.c    # Tokenizer unit tests
│   ├── test_parser.c       # Parser unit tests
│   ├── test_asm.c          # ASM tests
│   ├── test_asm_debug.c    # ASM debug tests
│   ├── test_asm_mini.c     # ASM mini tests
│   ├── test_reg.c          # Register tests
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
├── LLVM_BACKEND.md         # LLVM backend architecture & design (REMOVED — C transpiler is default)
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
            → C Transpiler (C source via c_transpiler/)            ✅
              → gcc/clang Compilation (native binary)              ✅
                → Binary Output (ELF64/Mach-O/PE32+/flat binary)   ✅
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

### `src/codegen/` — NASM Codegen Modules (15 files)

Used for `--target asm-x86_64|asm-arm64|asm-riscv64` targets. The default backend is the C transpiler (`src/c_transpiler/`).

| Module | Lines | Purpose |
|--------|-------|---------|
| `codegen_init.c` | 46 | Codegen create/destroy |
| `codegen_expr.c` | ~1200 | Expression codegen |
| `codegen_stmt.c` | ~850 | Statement codegen |
| `codegen_func.c` | ~200 | Function codegen |
| `codegen_top.c` | ~900 | Top-level codegen |
| `codegen_target.c` | ~85 | Target setup |
| `codegen_output.c` | ~95 | Output helpers |
| `codegen_frame.c` | ~200 | Stack frame |
| `codegen_defer.c` | ~30 | Defer |
| `codegen_aelib.c` | ~450 | Aelib metadata |
| `codegen_assemble.c` | ~400 | Assemble/link |
| `codegen_enum_layout.c` | ~55 | Enum layout |
| `codegen_type_helpers.c` | ~180 | Type helpers |
| `codegen_mem_map.c` | ~75 | Memory map |
| `codegen_internal.h` | 189 | Internal header |

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

8. **C transpiler is the default backend**: The C transpiler (`src/c_transpiler/`) handles all non-asm targets. The NASM codegen (`src/codegen/`) is only used for `--target asm-x86_64|asm-arm64|asm-riscv64`. New features should be added to the C transpiler first.

---

## How to Contribute

### Adding a New Language Feature

1. **Tokenizer**: Add new keyword/token type in `tokenizer.c` and `tokenizer.h`
2. **AST**: Add new node type in `ast.h` and create helper in `ast.c`
3. **Parser**: Add parsing logic in `parser.c`
4. **Semantic**: Add type checking in `semantic.c`
5. **Codegen**: Add code emission in the appropriate `src/c_transpiler/*.c` module
6. **Optimizer**: Update DCE/optimization passes in `optimizer.c`
7. **Tests**: Add test fixture in `tests/fixtures/` and update `Makefile`
8. **Docs**: Update REQUIREMENTS.md, SPECIFICATION.md, STATUS.md, AGENTS.md

### Adding a New Compiler Target

1. Add target enum in `include/aether/codegen.h` (Target enum)
2. Add target triple in `src/codegen/codegen_target.c` (`llvm_target_triple()`)
3. Add entry point / syscall handling in `src/codegen/codegen_func.c`
4. Add emission logic in `src/codegen/codegen_target.c`
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
| 19 | ⚪ CANCELLED | LLVM backend migration (C transpiler is default backend) |
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
| `src/codegen/` (15 files) | ~200KB | NASM codegen (asm targets only) |
| `src/c_transpiler/` (11 files) | ~150KB | C transpiler (default backend) |
| `REQUIREMENTS.md` | 755 | Language requirements |
| `SPECIFICATION.md` | 2773 | Language specification |
| `STATUS.md` | 540 | Implementation status |
| `LLVM_BACKEND.md` | 780 | LLVM backend design |
