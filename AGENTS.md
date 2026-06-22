# Aether Compiler — AGENTS.md

> **Primary entry point for AI agents (Claude Code, Codex, Cursor, Copilot, etc.)**
> Read this first before making any changes. This file is kept up to date with the actual state of the codebase.

---

## Quick Facts

- **Language**: C11 (bootstrap), targeting Aether (self-hosting goal)
- **Output**: NASM assembly → ELF64/Mach-O/flat binary
- **Build**: `make` → `./build/aether`
- **Test**: `make test` (unit) + `make test-host` (native .ae fixtures)
- **Install**: `sudo make install` or `make install-local`
- **Source**: `/Volumes/Backup/Development/Project_Aether/compiler/`
- **Branch**: `feature/P05.10-segfault-handling` (active development)

---

## Project Structure

```
compiler/
├── src/                    # C source files (bootstrap compiler)
│   ├── aether.c            # CLI entry point, import resolution, pipeline orchestration
│   ├── tokenizer.c         # Keyword table, token type names, tokenization
│   ├── lexer.c             # Lexer stream (indentation engine, token advancement)
│   ├── ast.c               # AST node creation helpers
│   ├── parser.c            # Recursive descent + Pratt parser (1828 lines)
│   ├── semantic.c          # Type checking, name resolution, const evaluation
│   ├── codegen.c           # NASM code generation (2701 lines — largest file)
│   ├── optimizer.c         # DCE, constant folding, inlining, escape analysis
│   ├── asm_ir.c            # Multi-target assembler IR
│   ├── asm_parser.c        # NASM instruction parser
│   ├── asm_backend_x86_64.c  # x86_64 backend (passthrough)
│   ├── asm_backend_arm64.c   # ARM64 backend (instruction mapping)
│   ├── asm_backend_riscv64.c # RISC-V backend (instruction mapping)
│   ├── universal.c         # Universal binary (fat binary) generation
│   ├── arena.c             # Arena allocator
│   ├── str.c               # String view utilities
│   └── vector.c            # Dynamic array
├── include/aether/         # Header files (14 headers)
│   ├── defs.h              # Common definitions, StringView, AstNodeList
│   ├── ast.h               # All AST node types (50+), binary/unary ops
│   ├── tokenizer.h         # Token types, keyword table
│   ├── lexer.h             # Lexer state
│   ├── parser.h            # Parser state, precedence levels
│   ├── semantic.h          # Semantic analyzer
│   ├── codegen.h           # Codegen state, Target enum (12 targets)
│   ├── optimizer.h         # Optimizer config
│   ├── asm_ir.h            # AsmIR types
│   ├── asm_parser.h        # NASM parser
│   ├── universal.h         # Universal binary types
│   ├── arena.h             # Arena allocator
│   ├── str.h               # String view
│   └── vector.h            # Dynamic array
├── tests/                  # C test suite + .ae fixture programs
│   ├── test_tokenizer.c    # Tokenizer unit tests
│   ├── test_parser.c       # Parser unit tests
│   ├── test_asm.c          # Multi-target assembler tests
│   ├── test_asm_mini.c     # Minimal assembler tests
│   ├── test_asm_debug.c    # Assembler debug tests
│   ├── test_reg.c          # Register translation tests
│   ├── fixtures/           # .ae test programs (37 fixtures)
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
├── REQUIREMENTS.md         # Comprehensive language requirements (29 sections)
├── SPECIFICATION.md        # Full language specification with code examples
├── STATUS.md               # Implementation status (20 phases)
├── PHASE_*.md              # Phase-specific task breakdowns
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
  → Tokenizer (whitespace-aware, indent engine)
    → Parser (Pratt + recursive descent)
      → AST (50+ node types)
        → Import Resolution (read, parse, merge imported files)
          → Semantic Analysis (type checking, name resolution, borrow checking)
            → HIR → MIR (CFG with memory annotations)
              → Optimization (constant folding, DCE, inlining, escape analysis)
                → LIR (register allocation, instruction selection)
                  → Code Generation (NASM text output)
                    → Multi-Target Assembler (x86_64/ARM64/RISC-V)
                      → Binary Output (ELF64/Mach-O/PE32+/flat binary)
```

---

## Key Source Files — What Each Does

### `src/aether.c` (1198 lines) — CLI & Pipeline Orchestration
- Entry point: `main()` parses CLI args, dispatches to `aether_init`, `aether_build`, `aether_run`
- Pipeline: `aether_compile()` calls tokenize → parse → import_resolve → semantic → optimize → codegen → assemble
- Import resolution: `resolve_imports()` reads imported files, parses with shared arena, merges declarations
- CLI flags: `--target`, `-o`, `-L`, `-S`, `--dump-ast`, `--dump-tokens`, `-v`
- `aether_init()` scaffolds new projects
- `aether_run()` compiles + executes in one step

### `src/tokenizer.c` (900 lines) — Keyword Table & Tokenization
- `KEYWORDS[]` table: 75 keywords mapped to `TokenType` enums
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

### `src/parser.c` (1828 lines) — Recursive Descent + Pratt Parser
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

### `src/semantic.c` (558 lines) — Type Checking & Name Resolution
- Two-pass analysis: first pass declares all names, second pass visits bodies
- `const_eval_expr()`: compile-time constant evaluation (arithmetic, bitwise, logical, sizeof, alignof)
- `scope_lookup()` / `scope_declare()`: symbol table management
- `semantic_analyze()`: main entry point, walks AST and resolves types
- `resolve_types()`: type resolution and checking

### `src/codegen.c` (2701 lines) — NASM Code Generation (LARGEST FILE)
- `codegen_create()` / `codegen_set_target()`: codegen initialization
- `codegen_generate()`: walks AST and emits NASM assembly text
- `cg_stmt()`: statement codegen (let, if, while, for, return, defer, asm, try, match, etc.)
- `cg_expr()`: expression codegen (literals, idents, binary/unary ops, calls, indexing, field access)
- `cg_func_decl()`: function prologue/epilogue, stack frame, parameter passing
- `cg_class_decl()`: class layout, method dispatch, auto-destructor insertion
- `cg_asm_block()`: emits raw NASM text, handles extern hoisting
- `cg_string_interp()`: BIN_CONCAT chain codegen with `__aether_concat` calls
- `cg_emit_runtime()`: emits runtime helpers (`__aether_alloc`, `__aether_concat`, `__aether_itoa`)
- `codegen_assemble()`: multi-target assemble/link pipeline (NASM → .o → ELF/Mach-O)
- Target-specific: handles TARGET_HOST, TARGET_KERNEL, TARGET_BOOT, TARGET_MODULE, TARGET_BINARY, TARGET_UNIVERSAL
- `process_string_literal()`: escape sequence processing (\n, \t, \xNN, etc.)
- `is_string_expr()` / `is_numeric_expr()`: type detection for `+` concat behavior

### `src/optimizer.c` (844 lines) — Optimization Passes
- `optimize()`: runs all enabled passes in order
- `opt_constant_fold()`: constant folding and propagation
- `opt_dead_code_elim()`: DCE (two-pass for cross-file references)
- `opt_inline()`: function inlining (heuristic + @force_inline)
- `opt_escape_analysis()`: heap/stack promotion (placeholder)
- `opt_region_elision()`: arena elision (placeholder)
- `opt_devirtualize()`: dynamic→static dispatch conversion (placeholder)
- `opt_loop_unroll()`: loop unrolling (placeholder)
- `opt_memory_fusion()`: memory operation fusion (placeholder)

### `src/asm_*.c` — Multi-Target Assembler
- `asm_ir.c`: AsmIR types (instructions, operands, registers, addressing modes)
- `asm_parser.c`: Parses NASM instruction text into AsmIR
- `asm_backend_x86_64.c`: x86_64 passthrough backend
- `asm_backend_arm64.c`: ARM64 instruction mapping (register translation, instruction equivalents)
- `asm_backend_riscv64.c`: RISC-V instruction mapping

### `src/universal.c` — Universal Binary Generation
- Fat binary container format (MultiArchHeader, ArchEntry)
- CPU detection trampoline (CPUID EFLAGS.ID bit test)
- Dual compilation pipeline (NASM → IR → per-arch backends → merge)
- Shared section deduplication

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
| `asm-x86_64` | NASM text | — | Assembly listing |
| `asm-arm64` | ARM64 text | — | ARM64 listing |
| `asm-riscv64` | RISC-V text | — | RISC-V listing |
| `universal` | Multi-arch ELF | CPU detection | x86_64 + ARM64 |
| `universal-all` | Multi-arch ELF | CPU detection | All architectures |

---

## Runtime Helpers (auto-emitted by codegen)

| Function | Purpose | Notes |
|----------|---------|-------|
| `__aether_alloc(rdi: size)` | Bump allocator | mmap-backed on host, no-op on kernel |
| `__aether_free(rdi: ptr)` | No-op free | Bump allocator doesn't free individually |
| `__aether_concat(left, right)` | String concatenation | Allocates + copies both strings. Handles null pointers (treats as empty string). |
| `__aether_itoa(rdi: u64)` | u64 to decimal string | Allocates 21 bytes, **clobbers rcx** |
| `LA_concat_memcpy(dest, src, count)` | Byte copy helper | Used by concat |

---

## Known Technical Decisions & Pitfalls

### Critical — Read Before Making Changes

1. **Asm blocks must NOT contain `ret`**: The compiler handles function prologue/epilogue. If an asm block contains `ret`, the compiler detects it and suppresses its own default return. NASM treats `;` as comment, so `leave; ret` on one line only executes `leave`. Always put `leave` and `ret` on separate lines.

2. **`__aether_itoa` clobbers rcx**: Uses `div rcx` internally. Any caller must save `rcx` before calling itoa.

3. **NASM 64-bit mode scale factors**: Only 1, 2, 4, 8 are allowed. `*32` and `*8+4` are invalid and must be replaced with shift+add sequences.

4. **`+` operator does string concat when either operand is a string**: Detected at codegen time by `is_string_expr()`. Numeric addition when both are numbers. This is NOT a parser-level decision — it's a codegen-level decision.

5. **DCE must handle NODE_EXPR_STMT**: The DCE optimizer must keep `let` statements with function call, asm block, or binary op initializers even if the variable is unused. Two-pass DCE for cross-file references.

6. **Kernel must be built with `-O0`**: The optimizer can remove side-effectful calls. Use `-O0` for kernel builds.

7. **`cli` before kernel call**: In boot.ae, `cli` must be emitted before calling kernel_main. No IDT is set up, so any interrupt (e.g. hardware timer IRQ0) causes GPF → double fault → triple fault → CPU reset.

8. **Import resolution keeps source buffers alive**: Imported AST nodes' StringView fields point into the imported source buffer. The buffer must be kept alive for the entire compilation. Circular imports are detected and skipped.

9. **Two-pass semantic analysis**: First pass declares all top-level names. Second pass visits function bodies. This handles forward references across files.

10. **String interpolation builds BIN_CONCAT chains**: `"Hello {name}!"` becomes `BIN_CONCAT("Hello ", BIN_CONCAT(name, "!"))`. Numeric expressions are auto-converted via `__aether_itoa`.

11. **`_aether_entry` receives args in registers (SysV ABI)**: On Mach-O, `LC_MAIN` calls the entry point as a C function: `rdi=argc, rsi=argv`. The entry wrapper reads `[rsi+8]` for `argv[1]`, not from the raw stack. The segfault handler init (`_aether_initSegfault`) clobbers caller-saved registers (`rdi`, `rsi`), so args must be saved in callee-saved registers (`r14`, `r15`) before calling it.

12. **`__aether_concat` and `print()` must handle null string pointers**: When `main(inputString: string)` receives no args, `rdi=0, rsi=0`. The concat helper and print built-in must test for null before dereferencing. Null is treated as an empty string (len=0, no copy).

---

## How to Contribute

### Adding a New Language Feature

1. **Tokenizer**: Add new keyword/token type in `tokenizer.c` and `tokenizer.h`
2. **AST**: Add new node type in `ast.h` and create helper in `ast.c`
3. **Parser**: Add parsing logic in `parser.c`
4. **Semantic**: Add type checking in `semantic.c`
5. **Codegen**: Add code emission in `codegen.c`
6. **Optimizer**: Update DCE/optimization passes in `optimizer.c`
7. **Tests**: Add test fixture in `tests/fixtures/` and update `Makefile`
8. **Docs**: Update REQUIREMENTS.md, SPECIFICATION.md, STATUS.md, AGENTS.md

### Adding a New Compiler Target

1. Add target enum in `codegen.h` (Target enum)
2. Add target name in `codegen.c` (`target_name()`)
3. Add entry point / syscall handling in `codegen.c` (`cg_emit_entry_point()`)
4. Add assemble/link logic in `codegen.c` (`codegen_assemble()`)
5. Add CLI flag in `aether.c` (`parse_args()`)
6. Add test fixture and update Makefile

### Running Tests

```bash
make test           # Unit tests (tokenizer + parser + asm)
make test-host      # Native .ae fixture tests (37 tests)
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
| 8-12 | 🟢 COMPLETE | Multi-target assembler, optimization, universal binaries, kernel codegen, @layout |
| 13 | 🟢 COMPLETE | Language specification & requirements |
| 14 | 🟢 COMPLETE | OS boot & shell stabilization |
| 15 | 🟢 COMPLETE | String interpolation & imports |
| 16 | 🔴 NOT STARTED | OS memory & process management |
| 17 | 🔴 NOT STARTED | Concurrency & fibers |
| 18 | 🔴 NOT STARTED | Advanced OS integration |
| 19 | 🔴 NOT STARTED | Goal-oriented I/O & query fusion |
| 20 | 🔴 NOT STARTED | Protocol generation & hardware configuration |

See [STATUS.md](STATUS.md) for detailed per-phase checklists.

---

## Key Files Reference

| File | Lines | Purpose |
|------|-------|---------|
| `src/codegen.c` | 2701 | NASM code generation (largest file) |
| `src/parser.c` | 1828 | Recursive descent parser |
| `src/aether.c` | 1198 | CLI entry point, pipeline orchestration |
| `src/tokenizer.c` | 900 | Keyword table, tokenization |
| `src/optimizer.c` | 844 | Optimization passes |
| `src/semantic.c` | 558 | Type checking, name resolution |
| `include/aether/ast.h` | 547 | AST node types (50+) |
| `include/aether/codegen.h` | 79 | Codegen state, Target enum |
| `include/aether/parser.h` | 127 | Parser state, precedence levels |
| `REQUIREMENTS.md` | 2259 | Language requirements |
| `SPECIFICATION.md` | 2199 | Language specification |
| `STATUS.md` | 328 | Implementation status |
