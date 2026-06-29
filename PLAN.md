# Phase 20 — C Transpiler Backend

> **Branch:** `feature/P20.00-c-transpiler`
> **Goal:** Replace NASM codegen and LLVM backend with a modular C transpiler. Every `.ae` file compiles to C, then to native binary via any C11 compiler (gcc, clang, tcc, MSVC). Zero dependencies beyond a C compiler and standard library.

---

## Why

The NASM codegen is x86_64-only, 2700 lines, and hard to maintain. The LLVM backend is fragile (opaque pointer crashes, version churn, segfaults in `DataLayout`). A C transpiler is:

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

The C codegen replaces both `src/codegen.c` (NASM) and `src/llvm/` (LLVM). The frontend stays unchanged.

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
├── c_asm.c                  # 100 lines — Inline assembly blocks
├── c_error.c                # 150 lines — Error handling (try/catch/throw)
├── c_contract.c             # 80 lines  — Pre/post conditions
├── c_runtime.c              # 100 lines — Runtime helper declarations (print, alloc, concat)
└── c_target.c               # 150 lines — Target-specific emission (host, freestanding, kernel, boot)

Total: ~1830 lines across 12 files. Average: ~150 lines per file.
```

---

## Task Breakdown

### P20.01 — Project structure & build system

**Files:** `Makefile`, `src/c_transpiler/c_transpiler.h`, `src/c_transpiler/c_transpiler.c`

Create the module directory, header, and entry point. Update Makefile with C transpiler compilation rules. The entry point walks the AST and dispatches to sub-modules.

**Deliverable:** `make` builds the compiler with C transpiler support. `--c` flag invokes the transpiler.

### P20.02 — Type mapping (c_types.c)

**File:** `src/c_transpiler/c_types.c`

Map every Aether type to its C representation:

| Aether | C |
|--------|---|
| `u8` | `uint8_t` |
| `u16` | `uint16_t` |
| `u32` | `uint32_t` |
| `u64` | `uint64_t` |
| `i8` | `int8_t` |
| `i16` | `int16_t` |
| `i32` | `int32_t` |
| `i64` | `int64_t` |
| `f32` | `float` |
| `f64` | `double` |
| `bool` | `uint8_t` (0/1) |
| `byte` | `uint8_t` |
| `char` | `uint8_t` |
| `string` | `struct { uint64_t len; const char *ptr; }` |
| `[T; N]` | `T[N]` |
| `[T]` | `struct { T *ptr; uint64_t len; }` |
| `T?` | `struct { uint8_t has_value; T value; }` |
| `(T1, T2)` | `struct { T1 f0; T2 f1; }` |
| `*T` | `T*` |
| `ref T` | `T*` |
| `func(T): R` | `R(*)(T)` |

Emit `typedef` declarations for all types used in the program.

### P20.03 — Expression codegen (c_expr.c)

**File:** `src/c_transpiler/c_expr.c`

Emit C expressions for every Aether expression type:

- Literals: int, float, string, char, bool, none
- Identifiers: variable/function name lookup
- Binary ops: arithmetic, comparison, logical, bitwise, concat
- Unary ops: neg, not, bit_not, ref, deref, inc, dec, array_len
- Function calls: `func(args...)`
- Index expressions: `arr[i]`
- Field access: `obj.field`
- Cast: `(type)expr`
- Ternary: `cond ? then : else`
- Array literals: `{val1, val2, ...}`
- String interpolation: concat chain with itoa

### P20.04 — Statement codegen (c_stmt.c)

**File:** `src/c_transpiler/c_stmt.c`

Emit C statements for every Aether statement type:

- `let` declarations: `type name = value;`
- Assignment: `name = expr;`
- Compound assignment: `name += expr;`
- `if`/`elif`/`else`: `if (...) { } else if (...) { } else { }`
- `while`: `while (...) { }`
- `for` range: `for (i = start; i < end; i++)`
- `for` array: `for (i = 0; i < len; i++)`
- `return`: `return value;`
- `break`/`continue`: `break;` / `continue;`
- `defer`: push to stack, emit at scope exit
- Expression statements: `expr;`
- `match`: `if/else if` chain
- `unsafe` block: `{ }`
- `region` block: `{ }`

### P20.05 — Function codegen (c_func.c)

**File:** `src/c_transpiler/c_func.c`

Emit C function declarations and definitions:

- Function signature: `return_type name(params)`
- Parameter handling: type conversion, default values
- Entry block: variable declarations, prologue
- Exit block: implicit return for void, defer cleanup
- `throws` functions: return struct `{ value, error_tag }`
- `extern` functions: `extern` declaration
- `sys` functions: inline asm syscall wrapper
- `@entry` / `@export` attributes

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

Emit inline assembly blocks:

- Basic `asm { body }`: `__asm__("body");`
- `asm: (outputs) { body }`: extended asm with output operands
- Top-level asm: `__asm__(".globl ...");`

### P20.08 — Error handling (c_error.c)

**File:** `src/c_transpiler/c_error.c`

Emit try/catch/throw:

- `throw expr`: set error tag, longjmp or return error struct
- `try { body } catch(e) { handler }`: setjmp/longjmp or error struct check
- Cleanup tables for scope exit during unwinding

### P20.09 — Contract codegen (c_contract.c)

**File:** `src/c_transpiler/c_contract.c`

Emit pre/post condition checks:

- `@pre(condition)`: `if (!(condition)) { fprintf(stderr, "..."); exit(1); }`
- `@post(condition)`: same at function exit

### P20.10 — Runtime helpers (c_runtime.c)

**File:** `src/c_transpiler/c_runtime.c`

Emit runtime helper function declarations and implementations:

- `print(string)`: concatenate args, write to stdout
- `__aether_alloc(size)`: malloc wrapper
- `__aether_free(ptr)`: free wrapper
- `__aether_concat(left, right)`: string concatenation
- `__aether_itoa(value)`: u64 to decimal string

### P20.11 — Target emission (c_target.c)

**File:** `src/c_transpiler/c_target.c`

Handle target-specific output:

- `host`: standard C with `main()` entry point
- `freestanding`: `-ffreestanding`, no stdlib
- `kernel`: freestanding + custom entry point
- `boot`: freestanding + flat binary via objcopy
- Compiler invocation: `gcc -o output input.c`
- Cross-compilation: `clang --target=...`

### P20.12 — Wire up CLI & remove old backends

**File:** `src/aether.c`

- Add `--c` flag to invoke C transpiler
- Keep `--llvm` flag for backward compat (optional)
- Remove `src/llvm/` directory
- Remove `src/codegen.c` and `src/asm_*.c` (NASM backend)
- Update Makefile

### P20.13 — All test fixtures pass

**File:** `Makefile`

Run all test fixtures through C transpiler. Fix remaining failures.

---

## Verification

```bash
make clean && make
./build/aether --c tests/fixtures/hello.ae -o /tmp/hello
/tmp/hello; echo $?    # prints "Hello, World!" exits 42
./build/aether --c tests/fixtures/test_math.ae -o /tmp/test_math
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
| `c_asm.c` | 100 | Inline assembly |
| `c_error.c` | 150 | Error handling |
| `c_contract.c` | 80 | Contract codegen |
| `c_runtime.c` | 100 | Runtime helpers |
| `c_target.c` | 150 | Target emission |
| **Total** | **~1830** | |

---

## Files to Remove

| File | Reason |
|------|--------|
| `src/llvm/` (entire directory) | LLVM backend replaced by C transpiler |
| `src/codegen.c` | NASM codegen replaced by C transpiler |
| `src/asm_ir.c` | NASM IR generation |
| `src/asm_parser.c` | NASM parser |
| `src/asm_backend_x86_64.c` | x86_64 assembler |
| `src/asm_backend_arm64.c` | ARM64 assembler |
| `src/asm_backend_riscv64.c` | RISC-V assembler |
| `src/universal.c` | Universal binary support |
| `include/aether/codegen.h` | NASM codegen header |
| `include/aether/asm_ir.h` | NASM IR header |
| `include/aether/asm_parser.h` | NASM parser header |
| `include/aether/asm_backend.h` | NASM backend header |
| `include/aether/universal.h` | Universal binary header |
| `PLAN.md` | Old LLVM plan |
| `LLVM_BACKEND.md` | Old LLVM design doc |

## Files to Keep

| File | Reason |
|------|--------|
| `src/aether.c` | CLI entry point — add `--c` flag |
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
