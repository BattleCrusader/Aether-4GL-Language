#ifndef AETHER_LLVM_H
#define AETHER_LLVM_H

#include "defs.h"
#include "ast.h"
#include "arena.h"
#include "codegen.h"  /* for Target enum */

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
    int cleanup_depth;
} LlvmTryFrame;

/* ──────────────────────────────────────────────
 * Defer entry — tracks a deferred block for
 * scope-exit codegen (LIFO chain).
 * ────────────────────────────────────────────── */
typedef struct LlvmDeferEntry {
    AstNode *body;
    struct LlvmDeferEntry *next;
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

    /* Pre-created types for common Aether types */
    LLVMTypeRef string_type;    /* { i64, ptr } — Aether string struct */

    /* Arena for internal allocations */
    Arena *arena;
};

/* ──────────────────────────────────────────────
 * Public API — called from aether.c
 * ────────────────────────────────────────────── */

/* Create and initialize LLVM codegen */
LlvmCodegen *llvm_create(Arena *arena, Target target, int opt_level);

/* Generate LLVM IR for a complete program.
 * Returns true on success, false on error. */
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

/* Cleanup — frees LLVM objects (but not the arena) */
void llvm_destroy(LlvmCodegen *lc);

/* ──────────────────────────────────────────────
 * Internal helpers (used across LLVM modules)
 * ────────────────────────────────────────────── */

/* Type mapping */
LLVMTypeRef llvm_prim_type(LlvmCodegen *lc, PrimType prim);
LLVMTypeRef llvm_type_from_ast(LlvmCodegen *lc, AstNode *type_node);
LLVMTypeRef llvm_throws_return_type(LlvmCodegen *lc, AstNode *return_type);

/* Expression codegen */
LLVMValueRef llvm_cg_expr(LlvmCodegen *lc, AstNode *node);

/* Statement codegen */
void llvm_cg_stmt(LlvmCodegen *lc, AstNode *node);
void llvm_cg_block(LlvmCodegen *lc, AstNode *node);

/* Function codegen */
void llvm_cg_func_decl(LlvmCodegen *lc, AstNode *node);

/* Runtime helpers */
void llvm_declare_runtime(LlvmCodegen *lc);
LLVMValueRef llvm_call_alloc(LlvmCodegen *lc, LLVMValueRef size);
LLVMValueRef llvm_call_concat(LlvmCodegen *lc, LLVMValueRef left, LLVMValueRef right);
LLVMValueRef llvm_call_itoa(LlvmCodegen *lc, LLVMValueRef value);

/* Target info */
const char *llvm_target_triple(Target target);
const char *llvm_target_data_layout(Target target);

/* Symbol table helpers */
void llvm_declare_local(LlvmCodegen *lc, StringView name, LLVMValueRef value, LLVMTypeRef type, bool is_mut);
LLVMValueRef llvm_lookup_local(LlvmCodegen *lc, StringView name);
void llvm_declare_global(LlvmCodegen *lc, StringView name, LLVMValueRef value, LLVMTypeRef type);
LLVMValueRef llvm_lookup_global(LlvmCodegen *lc, StringView name);

#endif /* AETHER_LLVM_H */
