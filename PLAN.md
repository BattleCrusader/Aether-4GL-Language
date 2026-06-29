# Phase 20 — C Transpiler Backend

> **Branch:** `feature/P20.01-c-transpiler-default`
> **Goal:** Replace NASM codegen with a modular C transpiler. Every `.ae` file compiles to C, then to native binary via any C11 compiler (gcc, clang, tcc, MSVC). Zero dependencies beyond a C compiler and standard library. C transpiler is the **default** backend — no flag needed.

---

## Why

The NASM codegen is x86_64-only, 2700 lines, and hard to maintain. A C transpiler is:

- **Portable** — any C11 compiler, any architecture
- **Simple** — emit text, not IR. No opaque pointer issues.
- **Debuggable** — read the C output, compile it, step through it
- **Fast to build** — no LLVM linking, compiles in <1s
- **Bootstrap-friendly** — once Aether is self-hosting, the transpiler is the bootstrap bridge

---

## Architecture

```
Source (.ae)
  → Tokenizer (existing)          ✅
    → Parser (existing)            ✅
      → AST (existing)             ✅
        → Import Resolution        ✅
          → Semantic Analysis      ✅
            → C Codegen (NEW)      🔴
              → C Compiler (gcc/clang/tcc) → native binary
```

The C codegen replaces `src/codegen.c` (NASM). The frontend stays unchanged.

---

## Module Map

```
src/c_transpiler/
├── c_transpiler.h          # 100 lines — Main header: CCodegen state, public API
├── c_transpiler.c           # 80 lines  — Entry point, walks AST, dispatches
├── c_types.c                # 120 lines — Aether → C type mapping
├── c_expr.c                 # 300 lines — Expression codegen (literals, idents, ops, calls)
├── c_stmt.c                 # 300 lines — Statement codegen (let, if, while, for, return, defer)
├── c_func.c                 # 200 lines — Function codegen (decl, params, return types)
├── c_string.c               # 150 lines — String operations (concat, interpolation, itoa)
├── c_asm.c                  # 150 lines — NASM→GCC asm converter (Intel syntax → extended asm)
├── c_error.c                # 150 lines — Error handling (try/catch/throw)
├── c_contract.c             # 80 lines  — Pre/post conditions
├── c_runtime.c              # 100 lines — Runtime helper declarations (print, alloc, concat)
└── c_target.c               # 150 lines — Target-specific emission (host, freestanding, kernel, boot)

Total: ~1880 lines across 12 files. Average: ~157 lines per file.
```

---

## Task Breakdown

### P20.01 — C transpiler is the default backend

**Files:** `src/aether.c`, `Makefile`

- Remove `--c` flag — C transpiler is always used
- Remove `--llvm` flag and all LLVM references
- Remove NASM codegen path from `aether.c`
- Remove unused CLI flags (`--dump-opt`, `-S` for asm listing, etc.)
- Keep `--target lib` for .aelib library output
- Update Makefile to only build C transpiler modules

### P20.02 — Type mapping (c_types.c)

**File:** `src/c_transpiler/c_types.c`

Map every Aether type to its C representation. Emit `typedef` declarations for all types used in the program.

### P20.03 — Expression codegen (c_expr.c)

**File:** `src/c_transpiler/c_expr.c`

Emit C expressions for every Aether expression type. Already implemented for: literals, idents, binary/unary ops, calls, indexing, field access.

### P20.04 — Statement codegen (c_stmt.c)

**File:** `src/c_transpiler/c_stmt.c`

Emit C statements for every Aether statement type. Already implemented for: let, if/elif/else, while, for, return, break/continue, defer, expr stmt.

### P20.05 — Function codegen (c_func.c)

**File:** `src/c_transpiler/c_func.c`

Emit C function declarations and definitions. Already implemented with `main()` → `int main()` conversion.

### P20.06 — String operations (c_string.c)

**File:** `src/c_transpiler/c_string.c`

Emit string operation code:
- String literal: `{ len, "literal" }` struct initializer
- String concat: call `__aether_concat(left, right)`
- String interpolation: build concat chain with itoa
- `__aether_itoa`: u64 → decimal string
- String indexing: `s.ptr[i]`
- String length: `s.len`

### P20.07 — Inline assembly (c_asm.c)

**File:** `src/c_transpiler/c_asm.c`

Convert Aether inline assembly (Intel/NASM syntax) to GCC extended asm.

**Why keep NASM syntax:** Intel assembly notation is significantly easier to read and write than GCC's AT&T syntax. Aether users write `asm { mov rax, 1; syscall }` — the transpiler converts this to `__asm__ volatile("mov %rax, %1; syscall" : "=a"(out) : "r"(val))`.

**Approach:**
- Reuse the existing `src/asm_parser.c` and `src/asm_ir.c` to parse NASM text into an IR
- `c_asm.c` converts the ASM IR to GCC extended asm format
- Basic `asm { body }` → `__asm__("body");`
- `asm: (outputs) { body }` → extended asm with output operand constraints
- Top-level asm → `__asm__(".globl ...");`

**NASM → GCC asm mapping:**

| NASM | GCC extended asm |
|------|-----------------|
| `mov rax, 1` | `mov %rax, $1` (or immediate) |
| `syscall` | `syscall` |
| `add rax, rbx` | `add %rax, %rbx` |
| `[rax + rbx*4]` | `(%rax, %rbx, 4)` |
| `.globl _start` | `.globl _start` (same) |
| `section .text` | `.text` (same) |

### P20.08 — Error handling (c_error.c)

**File:** `src/c_transpiler/c_error.c`

Emit try/catch/throw:
- `throw expr`: set error tag, return error struct
- `try { body } catch(e) { handler }`: check error tag after call
- Cleanup tables for scope exit during unwinding

### P20.09 — Contract codegen (c_contract.c)

**File:** `src/c_transpiler/c_contract.c`

Emit pre/post condition checks:
- `@pre(condition)`: `if (!(condition)) { fprintf(stderr, "..."); exit(1); }`
- `@post(condition)`: same at function exit

### P20.10 — Runtime helpers (c_runtime.c)

**File:** `src/c_transpiler/c_runtime.c`

Emit runtime helper function declarations and implementations. Already implemented.

### P20.11 — Target emission (c_target.c)

**File:** `src/c_transpiler/c_target.c`

Handle target-specific output:
- `host`: standard C with `main()` entry point
- `freestanding`: `-ffreestanding`, no stdlib
- `kernel`: freestanding + custom entry point
- `boot`: freestanding + flat binary via objcopy
- `lib`: .aelib library output (keep existing aelib.c)
- Compiler invocation: `gcc -o output input.c`
- Cross-compilation: `clang --target=...`

### P20.12 — Remove old NASM backend

**Files:** `src/codegen.c`, `src/asm_*.c`, `src/universal.c`, `include/aether/codegen.h`, `include/aether/asm_*.h`, `include/aether/universal.h`

Remove the old NASM codegen files. Keep `src/asm_parser.c` and `src/asm_ir.c` for the inline assembly converter (c_asm.c).

### P20.13 — All test fixtures pass

**File:** `Makefile`

Run all test fixtures through C transpiler. Fix remaining failures.

---

## Verification

```bash
make clean && make
./build/aether tests/fixtures/hello.ae -o /tmp/hello
/tmp/hello; echo $?    # exits 42
./build/aether tests/fixtures/test_math.ae -o /tmp/test_math
/tmp/test_math; echo $?  # exits 0
make test-c             # run all fixtures through C transpiler
```

---

## File Size Budget

| File | Lines | Purpose |
|------|-------|---------|
| `c_transpiler.h` | 100 | Main header, CCodegen state |
| `c_transpiler.c` | 80 | Entry point, AST walker |
| `c_types.c` | 120 | Type mapping |
| `c_expr.c` | 300 | Expression codegen |
| `c_stmt.c` | 300 | Statement codegen |
| `c_func.c` | 200 | Function codegen |
| `c_string.c` | 150 | String operations |
| `c_asm.c` | 150 | NASM→GCC asm converter |
| `c_error.c` | 150 | Error handling |
| `c_contract.c` | 80 | Contract codegen |
| `c_runtime.c` | 100 | Runtime helpers |
| `c_target.c` | 150 | Target emission |
| **Total** | **~1880** | |

---

## Files to Remove

| File | Reason |
|------|--------|
| `src/codegen.c` | NASM codegen replaced by C transpiler |
| `src/asm_backend_x86_64.c` | x86_64 assembler (NASM) |
| `src/asm_backend_arm64.c` | ARM64 assembler (NASM) |
| `src/asm_backend_riscv64.c` | RISC-V assembler (NASM) |
| `src/universal.c` | Universal binary support (NASM) |
| `include/aether/codegen.h` | NASM codegen header |
| `include/aether/asm_backend.h` | NASM backend header |
| `include/aether/universal.h` | Universal binary header |
| `LLVM_BACKEND.md` | Old LLVM design doc |

## Files to Keep

| File | Reason |
|------|--------|
| `src/aether.c` | CLI entry point — C transpiler is default |
| `src/asm_parser.c` | NASM parser — reused by c_asm.c for inline asm |
| `src/asm_ir.c` | ASM IR — reused by c_asm.c for inline asm |
| `include/aether/asm_ir.h` | ASM IR header — reused by c_asm.c |
| `include/aether/asm_parser.h` | ASM parser header — reused by c_asm.c |
| `src/aelib.c` | .aelib library format — keep for stdlib packaging |
| `include/aether/aelib.h` | .aelib header — keep |
| `src/tokenizer.c` | Tokenizer — unchanged |
| `src/lexer.c` | Lexer — unchanged |
| `src/ast.c` | AST helpers — unchanged |
| `src/parser.c` | Parser — unchanged |
| `src/semantic.c` | Semantic analysis — unchanged |
| `src/optimizer.c` | Optimizer — unchanged |
| `src/arena.c` | Arena allocator — unchanged |
| `src/str.c` | String view — unchanged |
| `src/vector.c` | Dynamic array — unchanged |
| `include/aether/defs.h` | Common definitions |
| `include/aether/ast.h` | AST node types |
| `include/aether/tokenizer.h` | Token types |
| `include/aether/lexer.h` | Lexer state |
| `include/aether/parser.h` | Parser state |
| `include/aether/semantic.h` | Semantic analyzer |
| `include/aether/optimizer.h` | Optimizer config |
| `include/aether/arena.h` | Arena allocator |
| `include/aether/str.h` | String view |
| `include/aether/vector.h` | Dynamic array |
| `std/*.ae` | Standard library |
| `tests/*` | Test suite |
| `Makefile` | Build system — update |
| `AGENTS.md` | Agent guide — update |
| `STATUS.md` | Status — update |
| `SPECIFICATION.md` | Language spec |
| `REQUIREMENTS.md` | Requirements |
| `CONTRIBUTING.md` | Contributing guide |
