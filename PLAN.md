# Phase 19 — Week 1: LLVM Backend Skeleton

> **Branch:** `feature/P19.00-llvm-backend`
> **Goal:** `aether --llvm run tests/fixtures/hello.ae` produces a working binary that prints 42 and exits with code 42.

---

## Overview

Week 1 establishes the LLVM backend skeleton. We create the core infrastructure — LLVM context, type mapping, and enough codegen to compile `hello.ae` end-to-end. The frontend (tokenizer, parser, AST, semantic analysis) is unchanged. Only the codegen path changes.

**What `hello.ae` looks like:**
```aether
func main(): u64 {
    print("Hello, World!\n")
    return 42
}
```

This exercises: function declaration, string literal, function call (`print`), return with value.

---

## Task Breakdown

### P19.01 — Create `include/aether/llvm.h`

**File:** `include/aether/llvm.h`
**Lines:** ~100

The main header defining the `LlvmCodegen` struct and public API. Every LLVM module includes this.

**Contents:**
- `LlvmCodegen` struct with: `context`, `module`, `builder`, `current_func`, `current_block`, symbol tables, loop/try stacks, defer chain, target config, debug info state
- `LlvmSymbol` struct — maps Aether name to `LLVMValueRef` + type
- `LlvmLoopFrame` — break/continue target blocks
- `LlvmTryFrame` — catch/finally target blocks
- `LlvmDeferEntry` — LIFO defer chain
- Public API declarations:
  - `llvm_create()`, `llvm_destroy()`
  - `llvm_generate()`
  - `llvm_emit_object()`, `llvm_emit_assembly()`, `llvm_emit_binary()`

**Depends on:** Nothing (standalone header)
**Test:** Compile check — `#include "aether/llvm.h"` compiles clean

### P19.02 — Create `src/llvm/llvm_init.c`

**File:** `src/llvm/llvm_init.c`
**Lines:** ~80

LLVM bootstrap — creates context, module, builder, initializes target registry.

**Functions:**
- `LlvmCodegen *llvm_create(Arena *arena, Target target, int opt_level)` — allocates struct, creates LLVM context/module/builder, sets target triple and data layout
- `void llvm_destroy(LlvmCodegen *lc)` — disposes LLVM objects (but not the arena — that's owned by the caller)

**Target triple mapping:**
- `TARGET_HOST` → auto-detected (macOS: `x86_64-apple-macosx`, Linux: `x86_64-unknown-linux-gnu`)
- `TARGET_KERNEL` → `x86_64-unknown-none-elf`
- `TARGET_BOOT` → `x86_64-unknown-none-elf`
- `TARGET_FREESTANDING` → `x86_64-unknown-none-elf`
- `TARGET_MODULE` → `x86_64-unknown-none-elf`
- `TARGET_BINARY` → `x86_64-unknown-none-elf`

**Depends on:** `llvm.h`
**Test:** `llvm_create()` returns non-NULL, module has correct target triple

### P19.03 — Create `src/llvm/llvm_types.c`

**File:** `src/llvm/llvm_types.c`
**Lines:** ~120

Maps Aether primitive types to LLVM types.

**Functions:**
- `LLVMTypeRef llvm_prim_type(LlvmCodegen *lc, PrimType prim)` — maps `PRIM_U64` → `LLVMInt64Type()`, `PRIM_BOOL` → `LLVMInt1Type()`, `PRIM_STRING` → pointer to i8, etc.
- `LLVMTypeRef llvm_type_from_ast(LlvmCodegen *lc, AstNode *type_node)` — handles compound types (arrays, slices, pointers, refs, optionals, tuples, function types)
- `LLVMTypeRef llvm_throws_return_type(LlvmCodegen *lc, AstNode *return_type)` — struct `{ value_type, i8 }` for throws functions

**Type mapping table:**
| Aether | LLVM |
|--------|------|
| `void` | `LLVMVoidType()` |
| `bool` | `LLVMInt1Type()` |
| `byte`, `u8`, `char` | `LLVMInt8Type()` |
| `u16` | `LLVMInt16Type()` |
| `u32` | `LLVMInt32Type()` |
| `u64`, `int` | `LLVMInt64Type()` |
| `i8` | `LLVMInt8Type()` |
| `i16` | `LLVMInt16Type()` |
| `i32` | `LLVMInt32Type()` |
| `i64` | `LLVMInt64Type()` |
| `f32`, `float` | `LLVMFloatType()` |
| `f64`, `double` | `LLVMDoubleType()` |
| `string` | `LLVMPointerType(LLVMInt8Type(), 0)` |

**Depends on:** `llvm.h`
**Test:** Each primitive maps to the correct LLVM type

### P19.04 — Create `src/llvm/llvm_expr.c` (minimal)

**File:** `src/llvm/llvm_expr.c`
**Lines:** ~100 (Week 1 — expanded in Week 2)

Minimal expression codegen — just enough for `hello.ae`.

**Functions (Week 1):**
- `LLVMValueRef llvm_cg_expr(LlvmCodegen *lc, AstNode *node)` — dispatcher
- `llvm_cg_literal_int()` — `LLVMConstInt()`
- `llvm_cg_literal_string()` — global string constant + GEP
- `llvm_cg_ident()` — local/global lookup + load
- `llvm_cg_call()` — `LLVMBuildCall2()` for `print()` built-in

**Depends on:** `llvm.h`, `llvm_types.c`
**Test:** `llvm_cg_expr(lc, literal_int(42))` returns `LLVMConstInt(i64, 42)`

### P19.05 — Create `src/llvm/llvm_func.c` (minimal)

**File:** `src/llvm/llvm_func.c`
**Lines:** ~100 (Week 1 — expanded in Week 2)

Minimal function codegen — just enough for `hello.ae`.

**Functions (Week 1):**
- `void llvm_cg_func_decl(LlvmCodegen *lc, AstNode *node)` — creates LLVM function, entry block, alloca for params, generates body, emits return
- `void llvm_cg_block(LlvmCodegen *lc, AstNode *node)` — iterates statements

**Depends on:** `llvm.h`, `llvm_types.c`, `llvm_expr.c`
**Test:** A function with `return 42` produces LLVM IR with `ret i64 42`

### P19.06 — Create `src/llvm/llvm_runtime.c` (minimal)

**File:** `src/llvm/llvm_runtime.c`
**Lines:** ~80

Declares runtime helper functions that the generated code calls.

**Functions:**
- `void llvm_declare_runtime(LlvmCodegen *lc)` — declares `__aether_alloc`, `__aether_concat`, `__aether_itoa` as external LLVM functions with correct signatures
- `LLVMValueRef llvm_call_alloc(LlvmCodegen *lc, LLVMValueRef size)` — generates call to `__aether_alloc`
- `LLVMValueRef llvm_call_concat(LlvmCodegen *lc, LLVMValueRef left, LLVMValueRef right)` — generates call to `__aether_concat`

**Runtime function signatures:**
```
__aether_alloc(i64) -> ptr         ; bump allocator
__aether_concat(ptr, ptr) -> ptr   ; string concat
__aether_itoa(i64) -> ptr          ; u64 to decimal string
```

**Depends on:** `llvm.h`
**Test:** Runtime declarations exist in module with correct signatures

### P19.07 — Create `src/llvm/llvm_target.c` (minimal)

**File:** `src/llvm/llvm_target.c`
**Lines:** ~100 (Week 1 — expanded in Week 5)

Minimal target emission — just enough to produce a `.o` file and link it.

**Functions (Week 1):**
- `const char *llvm_target_triple(Target target)` — returns target triple string
- `const char *llvm_target_data_layout(Target target)` — returns data layout string
- `int llvm_emit_object(LlvmCodegen *lc, const char *path)` — creates target machine, runs optimization, emits `.o` file
- `int llvm_emit_assembly(LlvmCodegen *lc, const char *path)` — emits assembly listing

**Depends on:** `llvm.h`
**Test:** `llvm_emit_object()` produces a valid `.o` file

### P19.08 — Wire up `--llvm` flag in `aether.c`

**File:** `src/aether.c`
**Lines:** ~20 changed

Add `--llvm` CLI flag that selects the LLVM codegen path instead of NASM.

**Changes:**
- Add `bool use_llvm` to CLI args parsing
- In `aether_compile()`: if `use_llvm`, call `llvm_create()` → `llvm_generate()` → `llvm_emit_object()` → link with system linker
- Keep NASM path as default for now

**Depends on:** All above modules
**Test:** `aether --llvm run hello.ae` produces output

### P19.09 — Update Makefile

**File:** `Makefile`
**Lines:** ~15 changed

Add LLVM compilation and linking.

**Changes:**
```
LLVM_CONFIG ?= /opt/homebrew/opt/llvm/bin/llvm-config
LLVM_CFLAGS := $(shell $(LLVM_CONFIG) --cflags)
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags --libs --system-libs)

LLVM_SRCS = src/llvm/llvm_init.c src/llvm/llvm_types.c \
            src/llvm/llvm_expr.c src/llvm/llvm_func.c \
            src/llvm/llvm_runtime.c src/llvm/llvm_target.c

LLVM_OBJS = $(LLVM_SRCS:src/llvm/%.c=build/llvm/%.o)

build/llvm/%.o: src/llvm/%.c include/aether/llvm.h
    mkdir -p build/llvm
    $(CC) $(CFLAGS) $(LLVM_CFLAGS) -c -o $@ $<

build/aether: $(OBJS) $(LLVM_OBJS)
    $(CC) -o $@ $^ $(LDFLAGS) $(LLVM_LDFLAGS)
```

**Depends on:** All above modules
**Test:** `make clean && make` compiles clean with LLVM

---

## Verification

### Test: `hello.ae` compiles and runs

```bash
make clean && make
./build/aether --llvm tests/fixtures/hello.ae -o /tmp/hello_llvm
/tmp/hello_llvm
echo $?   # Should print 42
```

### Test: LLVM IR output is readable

```bash
./build/aether --llvm tests/fixtures/hello.ae -S -o /tmp/hello.ll
cat /tmp/hello.ll
```

Should show:
```llvm
define i64 @main() {
entry:
  %calltmp = call ptr @print(ptr @.str)
  ret i64 42
}
```

### Test: NASM path still works

```bash
./build/aether tests/fixtures/hello.ae -o /tmp/hello_nasm
/tmp/hello_nasm
echo $?   # Should also print 42
```

---

## File Size Budget (Week 1)

| File | Lines | Status |
|------|-------|--------|
| `include/aether/llvm.h` | 100 | New |
| `src/llvm/llvm_init.c` | 80 | New |
| `src/llvm/llvm_types.c` | 120 | New |
| `src/llvm/llvm_expr.c` | 100 | New (minimal) |
| `src/llvm/llvm_func.c` | 100 | New (minimal) |
| `src/llvm/llvm_runtime.c` | 80 | New (minimal) |
| `src/llvm/llvm_target.c` | 100 | New (minimal) |
| `src/aether.c` | +20 | Modified |
| `Makefile` | +15 | Modified |
| **Total** | **~715** | **7 new files, 2 modified** |

---

## Dependencies

- LLVM 21.1.8 at `/opt/homebrew/opt/llvm/` (already installed)
- `llvm-config` at `/opt/homebrew/opt/llvm/bin/llvm-config`
- System linker (`ld` on macOS, `ld.lld` on Linux)
