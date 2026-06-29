# Phase 19 — Week 2: Expressions, Statements, and print()

> **Branch:** `feature/P19.01-llvm-week2`
> **Goal:** All simple test fixtures pass through the LLVM backend. `print()` works. If/while/for/assign/defer all generate correct LLVM IR.

---

## Overview

Week 2 expands the LLVM backend from "hello world only" to handling all common expression and statement types. The `print()` built-in is implemented via write syscall. Control flow (if, while, for) uses LLVM basic blocks. Assignment and compound assignment modify variables in place.

---

## Task Breakdown

### P19.10 — Implement `print()` built-in

**File:** `src/llvm/llvm_expr.c`
**Lines:** ~30 added

Replace the no-op `print()` with a real implementation that emits a write syscall.

**Approach:** For host targets, emit inline assembly that calls the macOS/Linux write syscall:
- macOS: `mov rax, 0x2000004; mov rdi, 1; mov rsi, str_ptr; mov rdx, len; syscall`
- Linux: `mov rax, 1; mov rdi, 1; mov rsi, str_ptr; mov rdx, len; syscall`

For now, use LLVM inline asm to emit the syscall. The string pointer and length are computed from the string argument.

**Test:** `aether --llvm run tests/fixtures/hello.ae` prints "Hello, World!" and exits with 42

### P19.11 — Unary operations

**File:** `src/llvm/llvm_expr.c`
**Lines:** ~40 added

Add codegen for all unary operators:
- `UNARY_NEG` → `LLVMBuildNeg()`
- `UNARY_NOT` → `LLVMBuildNot()` (logical not: `icmp eq` + `xor 1`)
- `UNARY_BIT_NOT` → `LLVMBuildNot()`
- `UNARY_INC` / `UNARY_DEC` → load, add/sub 1, store, return (prefix: new value, postfix: old value)
- `UNARY_ARRAY_LEN` → load first 8 bytes of array header
- `UNARY_REF` → return the alloca pointer
- `UNARY_DEREF` → load from pointer

**Test:** `let x = -5; let y = !true; let z = ~0xFF` all produce correct values

### P19.12 — If/elif/else

**File:** `src/llvm/llvm_stmt.c`
**Lines:** ~80 added

Implement if/elif/else using LLVM basic blocks:
1. Evaluate condition
2. Compare against zero (or use `icmp ne`)
3. `LLVMBuildCondBr()` to then block or else/elif chain
4. Then block: generate body, branch to merge block
5. Elif chain: each elif is a nested if with its own condition and blocks
6. Else block: generate body, branch to merge block
7. Merge block: continue after the if chain

**Test:** `tests/fixtures/test_if.ae` compiles and runs correctly

### P19.13 — While loops

**File:** `src/llvm/llvm_stmt.c`
**Lines:** ~40 added

Implement while loops:
1. Create condition block, body block, after block
2. Branch to condition block
3. Condition block: evaluate condition, `LLVMBuildCondBr()` to body or after
4. Body block: generate body, branch back to condition block
5. After block: continue

Push/pop loop context for break/continue support.

**Test:** `tests/fixtures/test_while.ae` compiles and runs correctly

### P19.14 — For loops

**File:** `src/llvm/llvm_stmt.c`
**Lines:** ~60 added

Implement for loops (range and array iteration):
- Range: `for i in 0..10` → create counter variable, compare, increment
- Array: `for val in arr` → create index variable, load element, increment
- With index: `for i, val in arr` → both index and value

**Test:** `tests/fixtures/test_for.ae` and `tests/fixtures/test_for_index.ae` compile and run

### P19.15 — Assignment (regular and compound)

**File:** `src/llvm/llvm_stmt.c`
**Lines:** ~50 added

Implement assignment statements:
- Regular: `x = expr` → evaluate expr, store to x's alloca
- Compound: `x += expr`, `x -= expr`, etc. → load x, apply op, store back
- Handle field access targets: `obj.field = expr` → GEP + store

**Test:** `let mut x = 5; x += 3;` produces x = 8

### P19.16 — Defer

**File:** `src/llvm/llvm_stmt.c`
**Lines:** ~40 added

Implement defer:
1. Push deferred body onto `lc->defer_head` (LIFO)
2. At scope exit (before return, at end of block), pop and emit deferred bodies
3. Deferred bodies are emitted as regular statement codegen

**Test:** `tests/fixtures/test_defer.ae` compiles and runs correctly

### P19.17 — Index expressions and field access

**File:** `src/llvm/llvm_expr.c`
**Lines:** ~50 added

Implement:
- `arr[i]` → GEP into array pointer + load
- `s[i]` (string indexing) → GEP into string + load byte
- `obj.field` → GEP into struct + load
- `obj.field = expr` → GEP into struct + store (in assignment codegen)

**Test:** Array indexing and struct field access work correctly

### P19.18 — Test all simple fixtures

**File:** `Makefile` (test target)
**Lines:** ~10 added

Add an `llvm-test` target that runs all simple test fixtures through the LLVM backend.

**Test fixtures to verify:**
- `hello.ae` — basic function + return
- `test_if.ae` — if/elif/else
- `test_while.ae` — while loops
- `test_for.ae` — for loops
- `test_for_index.ae` — for with index
- `test_defer.ae` — defer
- `test_assign.ae` — assignment
- `test_math.ae` — arithmetic
- `test_bool.ae` — boolean logic

---

## Verification

```bash
make clean && make
./build/aether --llvm run tests/fixtures/hello.ae    # prints "Hello, World!" exits 42
./build/aether --llvm run tests/fixtures/test_if.ae   # exits 0
./build/aether --llvm run tests/fixtures/test_math.ae  # exits 0
```

---

## File Size Budget (Week 2)

| File | Lines added | Total |
|------|-------------|-------|
| `src/llvm/llvm_expr.c` | +120 | ~220 |
| `src/llvm/llvm_stmt.c` | +310 | ~310 |
| `src/llvm/llvm_func.c` | +20 | ~120 |
| `src/llvm/llvm_sym.c` | +10 | ~90 |
| `Makefile` | +10 | — |
| **Total** | **~470** | |
