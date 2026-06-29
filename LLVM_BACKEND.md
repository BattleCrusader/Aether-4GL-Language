# LLVM Backend — Architecture & Design

> **Status**: Design — not yet implemented
> **Goal**: Replace `src/codegen.c` (2700-line NASM monolith) with a modular LLVM IR backend. Every `.ae` file, test fixture, and Makefile target stays unchanged.

---

## Design Principles

1. **One concern per file** — No file over 400 lines. Each module does one thing.
2. **Testable in isolation** — Every module has a clear API that can be unit-tested.
3. **Extendable by addition** — Adding a new AST node type means adding one function in one file. No switch-statement sprawl.
4. **Readable first** — Clear naming, consistent patterns, doc comments on every public function.
5. **Zero global state** — All state lives in `LlvmCodegen`, passed explicitly.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Frontend (unchanged)                      │
│                                                             │
│  Source (.ae)                                               │
│    → Tokenizer (whitespace-aware, indent engine)             │
│      → Parser (Pratt + recursive descent)                    │
│        → AST (50+ node types)                               │
│          → Import Resolution (read, parse, merge)            │
│            → Semantic Analysis (type checking, name resolve) │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              LLVM Codegen (src/llvm/*.c)                     │
│                                                             │
│  Walks AST, builds LLVM IR via LLVM-C API                    │
│                                                             │
│  llvm_expr.c    → expressions (literals, ops, calls, etc.)  │
│  llvm_stmt.c    → statements (let, if, while, for, return)   │
│  llvm_func.c    → function declarations and externs          │
│  llvm_string.c  → string concat, interpolation, itoa         │
│  llvm_asm.c     → inline assembly blocks                     │
│  llvm_error.c   → try/catch/throw, cleanup tables            │
│  llvm_contract.c → pre/post condition checks                  │
│  llvm_runtime.c → __aether_alloc, __aether_concat, etc.      │
│  llvm_debug.c   → DWARF debug info via LLVM metadata         │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                    LLVM Backend                              │
│                                                             │
│  llvm_target.c → target machine, optimization, emission      │
│                                                             │
│  LLVM IR → LLVMOptimizerRun() → optimization passes          │
│         → LLVMTargetMachineEmit() → object file or assembly  │
│         → llvm-objcopy -O binary → flat binary (boot)        │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                    Output Formats                            │
│                                                             │
│  ┌──────────────┬──────────────────┬──────────────────────┐ │
│  │ Target        │ Format            │ Pipeline             │ │
│  ├──────────────┼──────────────────┼──────────────────────┤ │
│  │ host          │ Mach-O / ELF64    │ LLVM → .o → ld       │ │
│  │ kernel        │ ELF64             │ LLVM → .o → lld + ld │ │
│  │ binary        │ ELF64             │ LLVM → .o → lld      │ │
│  │ module        │ ELF64 .ko         │ LLVM → .o → lld      │ │
│  │ boot          │ Flat binary       │ LLVM → .o → objcopy  │ │
│  │ freestanding  │ ELF64             │ LLVM → .o → lld      │ │
│  │ asm-x86_64    │ Intel assembly    │ LLVM → -S output     │ │
│  │ asm-arm64     │ ARM64 assembly    │ LLVM → -S output     │ │
│  │ asm-riscv64   │ RISC-V assembly   │ LLVM → -S output     │ │
│  │ wasm32        │ WebAssembly       │ LLVM → wasm-ld       │ │
│  └──────────────┴──────────────────┴──────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

**Key insight:** The frontend (everything above the dotted line) doesn't change at all. The LLVM codegen modules replace only the old `codegen.c` NASM text generator. The LLVM backend handles all target-specific output — we just tell it what format we want.

---

## Module Map

```
src/llvm/
├── llvm.h                  # 100 lines — Main header: LlvmCodegen state, public API
├── llvm_init.c             # 80 lines  — LLVM context/module/builder creation
├── llvm_types.c            # 120 lines — Aether → LLVM type mapping
├── llvm_expr.c             # 350 lines — Expression codegen (literals, idents, ops, calls, indexing)
├── llvm_stmt.c             # 300 lines — Statement codegen (let, if, while, for, return, defer, block)
├── llvm_func.c             # 250 lines — Function codegen (decl, params, entry/exit, variadics)
├── llvm_string.c           # 200 lines — String operations (concat, interpolation, itoa)
├── llvm_asm.c              # 150 lines — Inline assembly (asm blocks, output bindings)
├── llvm_error.c            # 200 lines — Error handling (throws, try/catch, cleanup tables)
├── llvm_contract.c         # 100 lines — Contract codegen (pre/post conditions)
├── llvm_runtime.c          # 150 lines — Runtime helpers (alloc, concat, itoa declarations)
├── llvm_target.c           # 200 lines — Target setup, object emission, linker scripts
└── llvm_debug.c            # 150 lines — Debug info (DWARF metadata, source locations)
```

**Total: ~2350 lines across 13 files. Average: ~180 lines per file.**

---

## Core State (`llvm.h`)

```c
#ifndef AETHER_LLVM_H
#define AETHER_LLVM_H

#include "ast.h"
#include "arena.h"
#include "codegen.h"  // for Target enum

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

/* ──────────────────────────────────────────────
 * Forward declarations
 * ────────────────────────────────────────────── */
typedef struct LlvmCodegen LlvmCodegen;

/* ──────────────────────────────────────────────
 * Symbol table entry — maps an Aether name to an
 * LLVM value (alloca slot, global, or function).
 * ────────────────────────────────────────────── */
typedef struct {
    StringView name;
    LLVMValueRef value;
    LLVMTypeRef type;       /* LLVM type of the value */
    bool is_mutable;
} LlvmSymbol;

/* ──────────────────────────────────────────────
 * Loop context — for break/continue target blocks
 * ────────────────────────────────────────────── */
typedef struct {
    LLVMBasicBlockRef break_block;
    LLVMBasicBlockRef continue_block;
} LlvmLoopFrame;

/* ──────────────────────────────────────────────
 * Try context — for catch/finally target blocks
 * ────────────────────────────────────────────── */
typedef struct {
    LLVMBasicBlockRef catch_block;
    LLVMBasicBlockRef finally_block;
    LLVMBasicBlockRef after_block;
    int cleanup_depth;      /* scope cleanup depth for this try */
} LlvmTryFrame;

/* ──────────────────────────────────────────────
 * Defer entry — tracks a deferred block for
 * scope-exit codegen.
 * ────────────────────────────────────────────── */
typedef struct LlvmDeferEntry {
    AstNode *body;
    struct LlvmDeferEntry *next;  /* LIFO chain */
} LlvmDeferEntry;

/* ──────────────────────────────────────────────
 * Main codegen state — passed to every function.
 * No globals. No singletons.
 * ────────────────────────────────────────────── */
struct LlvmCodegen {
    /* LLVM objects */
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;

    /* Current function */
    LLVMValueRef current_func;
    LLVMBasicBlockRef current_block;

    /* Symbol tables — simple dynamic arrays */
    LlvmSymbol *locals;         /* local variables + params */
    int local_count;
    int local_cap;
    LlvmSymbol *globals;        /* file-scope lets + consts */
    int global_count;
    int global_cap;

    /* Loop stack — for break/continue */
    LlvmLoopFrame *loop_stack;
    int loop_depth;

    /* Try stack — for catch/finally */
    LlvmTryFrame *try_stack;
    int try_depth;

    /* Defer chain — LIFO for current scope */
    LlvmDeferEntry *defer_head;

    /* Target configuration */
    Target target;
    int opt_level;              /* 0-3 */
    int64_t entry_addr;         /* from @entry(addr), 0 = default */
    bool has_layout;
    int64_t layout_start;
    int64_t layout_max;

    /* Source location tracking for debug info */
    const char *current_source_file;
    int current_line;
    int current_column;

    /* Arena for internal allocations */
    Arena *arena;
};

/* ──────────────────────────────────────────────
 * Public API — called from aether.c
 * ────────────────────────────────────────────── */

/* Create and initialize LLVM codegen */
LlvmCodegen *llvm_create(Arena *arena, Target target, int opt_level);

/* Generate LLVM IR for a complete program */
bool llvm_generate(LlvmCodegen *lc, AstNode *program);

/* Emit object file. Returns 0 on success. */
int llvm_emit_object(LlvmCodegen *lc, const char *output_path);

/* Emit assembly listing. Returns 0 on success. */
int llvm_emit_assembly(LlvmCodegen *lc, const char *output_path);

/* Emit flat binary (for boot targets). Returns 0 on success. */
int llvm_emit_binary(LlvmCodegen *lc, const char *output_path);

/* Generate linker script for @layout / @kernel_layout targets.
 * Returns heap-allocated string, caller must free. */
char *llvm_generate_linker_script(LlvmCodegen *lc);

/* Cleanup */
void llvm_destroy(LlvmCodegen *lc);

#endif /* AETHER_LLVM_H */
```

---

## Module Responsibilities

### `llvm_init.c` — LLVM Bootstrap

```c
/* ──────────────────────────────────────────────
 * llvm_init.c
 *
 * Creates the LLVM context, module, and builder.
 * Initializes the target registry for the given
 * target triple.
 * ────────────────────────────────────────────── */

LlvmCodegen *llvm_create(Arena *arena, Target target, int opt_level) {
    LlvmCodegen *lc = arena_alloc(arena, sizeof(LlvmCodegen));
    memset(lc, 0, sizeof(*lc));
    lc->arena = arena;
    lc->target = target;
    lc->opt_level = opt_level;

    /* Initialize LLVM */
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    /* Create context, module, builder */
    lc->context = LLVMContextCreate();
    lc->module = LLVMModuleCreateWithNameInContext("aether", lc->context);
    lc->builder = LLVMCreateBuilderInContext(lc->context);

    /* Set target triple based on Target enum */
    const char *triple = llvm_target_triple(target);
    LLVMSetTarget(lc->module, triple);

    /* Set data layout */
    const char *layout = llvm_target_data_layout(target);
    LLVMSetDataLayout(lc->module, layout);

    return lc;
}
```

### `llvm_types.c` — Type Mapping

```c
/* ──────────────────────────────────────────────
 * llvm_types.c
 *
 * Maps Aether types to LLVM types. Also handles
 * struct layouts, array types, function pointer
 * types, and optional types.
 *
 * Testable: call aether_type_to_llvm() with any
 * PrimType and verify the result.
 * ────────────────────────────────────────────── */

LLVMTypeRef llvm_prim_type(PrimType prim) {
    switch (prim) {
        case PRIM_VOID:   return LLVMVoidTypeInContext(lc->context);
        case PRIM_BOOL:   return LLVMInt1TypeInContext(lc->context);
        case PRIM_BYTE:
        case PRIM_U8:     return LLVMInt8TypeInContext(lc->context);
        case PRIM_U16:    return LLVMInt16TypeInContext(lc->context);
        case PRIM_U32:    return LLVMInt32TypeInContext(lc->context);
        case PRIM_U64:    return LLVMInt64TypeInContext(lc->context);
        case PRIM_I8:     return LLVMInt8TypeInContext(lc->context);
        case PRIM_I16:    return LLVMInt16TypeInContext(lc->context);
        case PRIM_I32:    return LLVMInt32TypeInContext(lc->context);
        case PRIM_I64:    return LLVMInt64TypeInContext(lc->context);
        case PRIM_F32:    return LLVMFloatTypeInContext(lc->context);
        case PRIM_F64:    return LLVMDoubleTypeInContext(lc->context);
        case PRIM_STRING: return LLVMPointerType(LLVMInt8TypeInContext(lc->context), 0);
    }
}

/* Convert an Aether type AST node to an LLVM type */
LLVMTypeRef llvm_type_from_ast(LlvmCodegen *lc, AstNode *type_node);

/* Get the LLVM type for a throws function return:
 *   { return_type, i8 }  — value + error tag */
LLVMStructTypeRef llvm_throws_return_type(LlvmCodegen *lc, AstNode *return_type);
```

### `llvm_expr.c` — Expression Codegen

```c
/* ──────────────────────────────────────────────
 * llvm_expr.c
 *
 * Generates LLVM IR for every expression type:
 *   - Literals (int, float, string, char, bool, none)
 *   - Identifiers (local lookup → global lookup)
 *   - Binary ops (arithmetic, comparison, logical, bitwise, concat)
 *   - Unary ops (neg, not, bit_not, ref, deref, inc, dec, array_len)
 *   - Function calls (regular, variadic, method calls)
 *   - Index expressions (array[i], string[i])
 *   - Field access (struct.field)
 *   - Cast expressions (x as Type)
 *   - Ternary/if-else expressions
 *   - Array literals
 *   - Lambda expressions
 *
 * Each expression type is a separate function:
 *   llvm_cg_literal_int()
 *   llvm_cg_literal_float()
 *   llvm_cg_ident()
 *   llvm_cg_binary_op()
 *   llvm_cg_unary_op()
 *   llvm_cg_call()
 *   llvm_cg_index()
 *   llvm_cg_field_access()
 *   llvm_cg_cast()
 *   llvm_cg_ternary()
 *   llvm_cg_array_lit()
 *   llvm_cg_lambda()
 *
 * Testable: build an AST node manually, call the
 * function, verify the returned LLVMValueRef type
 * and value.
 * ────────────────────────────────────────────── */

LLVMValueRef llvm_cg_expr(LlvmCodegen *lc, AstNode *node);
```

### `llvm_stmt.c` — Statement Codegen

```c
/* ──────────────────────────────────────────────
 * llvm_stmt.c
 *
 * Generates LLVM IR for every statement type:
 *   - Let declarations (with/without type annotation)
 *   - Assignment (regular, compound += -= etc.)
 *   - If/elif/else (including if-let)
 *   - While loops
 *   - For loops (range, array iteration, with index)
 *   - Return (with/without value, throws return)
 *   - Break/continue (with loop label support)
 *   - Defer (push to defer chain, emit at scope exit)
 *   - Expression statements (discard result)
 *   - Match statements
 *   - Unsafe blocks
 *   - Region blocks
 *
 * Each statement type is a separate function:
 *   llvm_cg_let()
 *   llvm_cg_assign()
 *   llvm_cg_if()
 *   llvm_cg_while()
 *   llvm_cg_for()
 *   llvm_cg_return()
 *   llvm_cg_break()
 *   llvm_cg_continue()
 *   llvm_cg_defer()
 *   llvm_cg_match()
 *   llvm_cg_unsafe()
 *   llvm_cg_region()
 *
 * Testable: build a statement AST node, call the
 * function, verify the IR output.
 * ────────────────────────────────────────────── */

void llvm_cg_stmt(LlvmCodegen *lc, AstNode *node);
```

### `llvm_func.c` — Function Codegen

```c
/* ──────────────────────────────────────────────
 * llvm_func.c
 *
 * Generates LLVM IR for function declarations:
 *   - Creates LLVM function with correct type
 *   - Sets up entry block and alloca slots for params
 *   - Handles variadic parameters (array packing)
 *   - Handles throws return type (struct { value, error_tag })
 *   - Emits function body
 *   - Inserts implicit return for void functions
 *   - Handles extern function declarations (no body)
 *   - Handles sys function declarations
 *   - Handles @entry, @export attributes
 *
 * Also handles:
 *   - Method declarations (implicit self param)
 *   - Constructor (init) and destructor (drop)
 *   - Operator overload methods
 *
 * Testable: declare a function AST node, call
 * llvm_cg_func_decl(), verify the LLVM function
 * exists with correct params and return type.
 * ────────────────────────────────────────────── */

void llvm_cg_func_decl(LlvmCodegen *lc, AstNode *node);
void llvm_cg_extern_decl(LlvmCodegen *lc, AstNode *node);
```

### `llvm_string.c` — String Operations

```c
/* ──────────────────────────────────────────────
 * llvm_string.c
 *
 * Handles all string-related codegen:
 *   - String literal creation (global constant + GEP)
 *   - String concatenation (BIN_CONCAT chains)
 *   - String interpolation (BIN_CONCAT + itoa for numerics)
 *   - __aether_itoa integration (u64 → string)
 *   - String indexing (s[i] → byte at offset)
 *   - String length (#s → strlen or stored length)
 *
 * The string representation is:
 *   [length: u64][data: ...bytes, null-terminated]
 *   Pointer points to the data (after the length header).
 *
 * Testable: create a string literal AST node, call
 * llvm_cg_string_literal(), verify the global constant
 * and GEP indices.
 * ────────────────────────────────────────────── */

LLVMValueRef llvm_cg_string_literal(LlvmCodegen *lc, StringView text);
LLVMValueRef llvm_cg_concat(LlvmCodegen *lc, AstNode *left, AstNode *right);
LLVMValueRef llvm_cg_itoa(LlvmCodegen *lc, LLVMValueRef value);
```

### `llvm_asm.c` — Inline Assembly

```c
/* ──────────────────────────────────────────────
 * llvm_asm.c
 *
 * Handles inline assembly blocks:
 *   - Basic asm { body } — no outputs, no clobbers
 *   - asm: (outputs) { body } — output bindings
 *   - Top-level asm blocks — emitted as module-level
 *     inline asm
 *   - Extern hoisting — extern declarations from asm
 *     blocks are collected and emitted at module level
 *
 * LLVM inline asm uses Intel syntax, same as NASM.
 * The asm text is passed verbatim to LLVM.
 *
 * Testable: create an asm block AST node, call
 * llvm_cg_asm_block(), verify the LLVM inline asm
 * call instruction.
 * ────────────────────────────────────────────── */

void llvm_cg_asm_block(LlvmCodegen *lc, AstNode *node);
void llvm_cg_asm_output(LlvmCodegen *lc, AstNode *node, LLVMValueRef *outputs, int count);
void llvm_cg_toplevel_asm(LlvmCodegen *lc, AstNode *node);
```

### `llvm_error.c` — Error Handling

```c
/* ──────────────────────────────────────────────
 * llvm_error.c
 *
 * Implements deterministic exceptions via LLVM:
 *   - Throws functions return { value, error_tag }
 *   - try/catch creates basic blocks for success/error paths
 *   - throw sets error_tag and branches to cleanup
 *   - Cleanup tables track live objects and defer blocks
 *   - finally blocks execute on both success and error paths
 *   - Nested try/catch is handled via the try_stack
 *   - Segfault handling: LLVM's trap instruction + signal handler
 *
 * Testable: create a try/catch AST, call llvm_cg_try(),
 * verify the basic block structure and phi nodes.
 * ────────────────────────────────────────────── */

void llvm_cg_try(LlvmCodegen *lc, AstNode *node);
void llvm_cg_throw(LlvmCodegen *lc, AstNode *node);
void llvm_cg_cleanup(LlvmCodegen *lc, int depth);
```

### `llvm_contract.c` — Contract Codegen

```c
/* ──────────────────────────────────────────────
 * llvm_contract.c
 *
 * Generates runtime checks for pre/post conditions:
 *   - pre(expr): evaluate expr at function entry,
 *     assert or branch to error handler
 *   - post(expr): evaluate expr at function exit,
 *     with old() values saved at entry
 *   - Debug mode: runtime checks enabled
 *   - Release mode: contracts are eliminated
 *     (optimizer removes dead code)
 *
 * Testable: create a function with pre/post conditions,
 * call llvm_cg_contracts(), verify the check code.
 * ────────────────────────────────────────────── */

void llvm_cg_preconditions(LlvmCodegen *lc, AstNodeList *pre);
void llvm_cg_postconditions(LlvmCodegen *lc, AstNodeList *post, LLVMValueRef return_value);
```

### `llvm_runtime.c` — Runtime Helpers

```c
/* ──────────────────────────────────────────────
 * llvm_runtime.c
 *
 * Declares and generates runtime helper functions
 * that the generated code calls:
 *
 *   __aether_alloc(size: u64) -> ptr
 *     Bump allocator, mmap-backed on host
 *
 *   __aether_free(ptr) -> void
 *     No-op (bump allocator doesn't free individually)
 *
 *   __aether_concat(left: ptr, right: ptr) -> ptr
 *     String concatenation, handles null as empty
 *
 *   __aether_itoa(value: u64) -> ptr
 *     Integer to decimal string
 *
 * These are declared as LLVM functions and either:
 *   a) Linked from a runtime library (libaether_rt.o)
 *   b) Inlined as LLVM IR (for small functions)
 *
 * Testable: verify the runtime function declarations
 * exist in the module with correct signatures.
 * ────────────────────────────────────────────── */

void llvm_declare_runtime(LlvmCodegen *lc);
LLVMValueRef llvm_call_alloc(LlvmCodegen *lc, LLVMValueRef size);
LLVMValueRef llvm_call_concat(LlvmCodegen *lc, LLVMValueRef left, LLVMValueRef right);
LLVMValueRef llvm_call_itoa(LlvmCodegen *lc, LLVMValueRef value);
```

### `llvm_target.c` — Target Setup & Emission

```c
/* ──────────────────────────────────────────────
 * llvm_target.c
 *
 * Handles target-specific configuration:
 *   - Maps Target enum to LLVM target triple
 *   - Creates LLVMTargetMachine
 *   - Emits object file (.o)
 *   - Emits assembly listing (.asm)
 *   - Emits flat binary (via llvm-objcopy)
 *   - Generates linker scripts for @layout targets
 *   - Handles @kernel_layout memory map verification
 *
 * Target triple mapping:
 *   TARGET_HOST        → auto-detected
 *   TARGET_KERNEL      → x86_64-unknown-none-elf
 *   TARGET_BOOT        → x86_64-unknown-none-elf
 *   TARGET_FREESTANDING → x86_64-unknown-none-elf
 *   TARGET_MODULE      → x86_64-unknown-none-elf
 *   TARGET_BINARY      → x86_64-unknown-none-elf
 *   TARGET_WASM32      → wasm32-unknown-unknown (future)
 *
 * Testable: call llvm_emit_object() with a valid
 * module, verify the .o file exists and has correct
 * format.
 * ────────────────────────────────────────────── */

const char *llvm_target_triple(Target target);
const char *llvm_target_data_layout(Target target);
int llvm_emit_object(LlvmCodegen *lc, const char *path);
int llvm_emit_assembly(LlvmCodegen *lc, const char *path);
int llvm_emit_binary(LlvmCodegen *lc, const char *path);
char *llvm_generate_linker_script(LlvmCodegen *lc);
```

### `llvm_debug.c` — Debug Information

```c
/* ──────────────────────────────────────────────
 * llvm_debug.c
 *
 * Generates DWARF debug information via LLVM
 * metadata:
 *   - Source file and line number for every
 *     instruction (LLVMSetCurrentDebugLocation)
 *   - Function scope info (LLVMDIBuilderCreateFunction)
 *   - Variable location info (LLVMDIBuilderCreateAutoVariable)
 *   - Type information (LLVMDIBuilderCreateBasicType)
 *
 * This replaces the current custom source map table
 * with proper DWARF, enabling:
 *   - lldb/gdb debugging
 *   - Stack traces with file:line
 *   - IDE integration (breakpoints, variable inspection)
 *
 * Testable: generate a simple function with debug
 * info, emit as object, verify with llvm-dwarfdump.
 * ────────────────────────────────────────────── */

void llvm_debug_init(LlvmCodegen *lc, const char *source_file);
void llvm_debug_set_location(LlvmCodegen *lc, int line, int column);
void llvm_debug_func_begin(LlvmCodegen *lc, AstNode *func);
void llvm_debug_func_end(LlvmCodegen *lc);
void llvm_debug_var(LlvmCodegen *lc, StringView name, LLVMValueRef alloca, AstNode *type);
```

---

## Integration with `aether.c`

The existing `aether.c` pipeline changes minimally:

```c
// In aether_compile():
if (use_llvm) {
    LlvmCodegen *lc = llvm_create(arena, target, opt_level);
    if (!llvm_generate(lc, program)) { error... }

    switch (target) {
        case TARGET_HOST:
        case TARGET_KERNEL:
        case TARGET_BINARY:
        case TARGET_MODULE:
            llvm_emit_object(lc, output_path);
            // Link with system linker or lld
            break;

        case TARGET_BOOT:
            llvm_emit_binary(lc, output_path);
            break;

        case TARGET_ASM_X86_64:
        case TARGET_ASM_ARM64:
        case TARGET_ASM_RISCV64:
            llvm_emit_assembly(lc, output_path);
            break;
    }

    llvm_destroy(lc);
} else {
    // Old NASM path (for transition period only)
    codegen_generate(...);
}
```

The `use_llvm` flag is set by:
- Default: `true` (LLVM is the primary backend)
- `--nasm` flag: `false` (fallback during transition)
- Auto-detect: `false` if LLVM not installed

---

## Build System

```makefile
# src/llvm/*.c
LLVM_SRCS = \
    src/llvm/llvm_init.c \
    src/llvm/llvm_types.c \
    src/llvm/llvm_expr.c \
    src/llvm/llvm_stmt.c \
    src/llvm/llvm_func.c \
    src/llvm/llvm_string.c \
    src/llvm/llvm_asm.c \
    src/llvm/llvm_error.c \
    src/llvm/llvm_contract.c \
    src/llvm/llvm_runtime.c \
    src/llvm/llvm_target.c \
    src/llvm/llvm_debug.c

LLVM_OBJS = $(LLVM_SRCS:src/llvm/%.c=build/llvm/%.o)

build/llvm/%.o: src/llvm/%.c include/aether/llvm.h
    mkdir -p build/llvm
    $(CC) $(CFLAGS) $(LLVM_CFLAGS) -c -o $@ $<

build/aether: $(OBJS) $(LLVM_OBJS)
    $(CC) -o $@ $^ $(LDFLAGS) $(LLVM_LDFLAGS)
```

---

## Testing Strategy

Each module has a corresponding test file:

```c
// tests/test_llvm_types.c
void test_prim_type_mapping() {
    LlvmCodegen *lc = llvm_create(test_arena, TARGET_HOST, 0);
    assert(llvm_prim_type(lc, PRIM_U64) == LLVMInt64TypeInContext(lc->context));
    assert(llvm_prim_type(lc, PRIM_BOOL) == LLVMInt1TypeInContext(lc->context));
    assert(llvm_prim_type(lc, PRIM_STRING) == LLVMPointerType(LLVMInt8TypeInContext(lc->context), 0));
    llvm_destroy(lc);
}

// tests/test_llvm_expr.c
void test_binary_add() {
    // Build: NODE_BINARY_OP(BIN_ADD, NODE_LITERAL_INT(1), NODE_LITERAL_INT(2))
    AstNode *one = node_literal_int(test_arena, 1);
    AstNode *two = node_literal_int(test_arena, 2);
    AstNode *add = node_binary_op(test_arena, BIN_ADD, one, two);

    LlvmCodegen *lc = llvm_create(test_arena, TARGET_HOST, 0);
    LLVMValueRef result = llvm_cg_expr(lc, add);

    // Verify: result is an LLVM add instruction
    assert(LLVMIsAInstruction(result));
    assert(LLVMGetInstructionOpcode(result) == LLVMAdd);
    llvm_destroy(lc);
}
```

Test files:
- `tests/test_llvm_types.c` — Type mapping correctness
- `tests/test_llvm_expr.c` — Expression codegen (one test per expression type)
- `tests/test_llvm_stmt.c` — Statement codegen
- `tests/test_llvm_func.c` — Function codegen
- `tests/test_llvm_string.c` — String operations
- `tests/test_llvm_asm.c` — Inline assembly
- `tests/test_llvm_error.c` — Error handling
- `tests/test_llvm_contract.c` — Contract codegen
- `tests/test_llvm_target.c` — Target emission

---

## File Size Budget

| File | Lines | Purpose |
|------|-------|---------|
| `include/aether/llvm.h` | 100 | Main header, state struct, public API |
| `src/llvm/llvm_init.c` | 80 | LLVM bootstrap |
| `src/llvm/llvm_types.c` | 120 | Type mapping |
| `src/llvm/llvm_expr.c` | 350 | Expression codegen (12 functions) |
| `src/llvm/llvm_stmt.c` | 300 | Statement codegen (12 functions) |
| `src/llvm/llvm_func.c` | 250 | Function codegen |
| `src/llvm/llvm_string.c` | 200 | String operations |
| `src/llvm/llvm_asm.c` | 150 | Inline assembly |
| `src/llvm/llvm_error.c` | 200 | Error handling |
| `src/llvm/llvm_contract.c` | 100 | Contract codegen |
| `src/llvm/llvm_runtime.c` | 150 | Runtime helpers |
| `src/llvm/llvm_target.c` | 200 | Target setup & emission |
| `src/llvm/llvm_debug.c` | 150 | Debug info |
| **Total** | **~2350** | **13 files, avg 180 lines each** |

---

## Migration Path

### Week 1: Skeleton + Expressions
1. Create `include/aether/llvm.h` and `src/llvm/llvm_init.c`
2. Create `src/llvm/llvm_types.c` — type mapping
3. Create `src/llvm/llvm_expr.c` — literals, idents, binary ops
4. Wire up in `aether.c` with `--llvm` flag
5. Test: `aether --llvm run tests/fixtures/test_hello.ae`

### Week 2: Statements + Functions
1. Create `src/llvm/llvm_stmt.c` — let, if, while, for, return
2. Create `src/llvm/llvm_func.c` — function declarations
3. Test: all simple test fixtures pass

### Week 3: Strings + Error Handling
1. Create `src/llvm/llvm_string.c` — concat, interpolation
2. Create `src/llvm/llvm_error.c` — try/catch/throw
3. Test: string and error test fixtures pass

### Week 4: Assembly + Contracts + Runtime
1. Create `src/llvm/llvm_asm.c` — inline asm
2. Create `src/llvm/llvm_contract.c` — pre/post
3. Create `src/llvm/llvm_runtime.c` — alloc, concat, itoa
4. Test: all test fixtures pass

### Week 5: Targets + Debug + Polish
1. Create `src/llvm/llvm_target.c` — object/assembly/binary emission
2. Create `src/llvm/llvm_debug.c` — DWARF debug info
3. Test: boot sector, kernel, multi-arch targets
4. Remove old `codegen.c` and `asm_*.c` files
5. Make LLVM the default, `--nasm` the fallback

---

## What Stays Unchanged

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| `src/aether.c` | 1200 | CLI, pipeline orchestration | Minor changes (add dispatcher) |
| `src/tokenizer.c` | 900 | Tokenization | Unchanged |
| `src/lexer.c` | ~200 | Lexer stream | Unchanged |
| `src/ast.c` | ~300 | AST node creation | Unchanged |
| `src/parser.c` | 1800 | Recursive descent parser | Unchanged |
| `src/semantic.c` | 550 | Type checking, name resolution | Unchanged |
| `src/optimizer.c` | 850 | Optimization passes | Unchanged (optional with LLVM) |
| `src/arena.c` | ~100 | Arena allocator | Unchanged |
| `src/str.c` | ~100 | String view utilities | Unchanged |
| `src/vector.c` | ~100 | Dynamic array | Unchanged |
| `include/aether/*.h` | ~1000 | All headers | New `llvm.h` added |
| `std/*.ae` | ~2000 | Standard library | Unchanged |
| `tests/*.c` | ~2000 | Test suite | New LLVM tests added |
| `tests/fixtures/*.ae` | ~2000 | Test fixtures | Unchanged |
| `Makefile` | ~200 | Build system | LLVM flags added |

**Total unchanged: ~12,000 lines of working code.**
**New code: ~2,350 lines across 13 files.**
**Deleted: ~4,500 lines (codegen.c + asm_*.c).**
