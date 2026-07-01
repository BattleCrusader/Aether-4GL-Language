#ifndef AETHER_C_TRANSPILER_H
#define AETHER_C_TRANSPILER_H

#include <stdio.h>
#include "defs.h"
#include "ast.h"
#include "arena.h"
#include "codegen.h"  /* for Target enum */

/* ──────────────────────────────────────────────
 * C codegen state — passed to every function.
 * No globals. No singletons.
 * ────────────────────────────────────────────── */
typedef struct {
    /* Output */
    FILE *out;

    /* Arena for internal allocations */
    Arena *arena;

    /* Target configuration */
    Target target;
    int opt_level;

    /* Current function context */
    const char *current_func_name;
    bool current_func_has_return;

    /* Indentation level */
    int indent;

    /* String counter for unique names */
    int str_counter;
    int label_counter;

    /* Defer counter for LIFO tracking */
    int defer_counter;

    /* Lambda counter for unique function names */
    int lambda_counter;

    /* Lambda function declarations collected during expression emission */
    AstNodeList lambda_decls;

    /* Auto-drop tracking for class-typed variables */
    struct AutoDropEntry {
        char var_name[64];
        char class_name[64];
        struct AutoDropEntry *next;
    } *auto_drop_list;

    /* Source location */
    const char *current_source_file;
    int current_line;

    /* Program AST for type lookups */
    AstNode *program;
} CCodegen;

/* ──────────────────────────────────────────────
 * Public API
 * ────────────────────────────────────────────── */

/* Create and initialize C codegen */
CCodegen *c_create(Arena *arena, Target target, int opt_level);

/* Generate C code for a complete program.
 * Returns true on success, false on error. */
bool c_generate(CCodegen *cg, AstNode *program, FILE *out);

/* ──────────────────────────────────────────────
 * Internal helpers (used across C modules)
 * ────────────────────────────────────────────── */

/* Indentation */
void c_indent(CCodegen *cg);
void c_newline(CCodegen *cg);

/* Type mapping */
void c_emit_type(CCodegen *cg, AstNode *type_node);
void c_emit_prim_type(CCodegen *cg, PrimType prim);
const char *c_type_name(AstNode *type_node);
const char *c_prim_type_name(PrimType prim);

/* Expression codegen */
void c_emit_expr(CCodegen *cg, AstNode *node);

/* Statement codegen */
void c_emit_stmt(CCodegen *cg, AstNode *node);
void c_emit_block(CCodegen *cg, AstNode *node);

/* Function codegen */
void c_emit_func_decl(CCodegen *cg, AstNode *node);

/* Runtime helpers */
void c_emit_runtime(CCodegen *cg);

/* Target emission */
void c_emit_target_preamble(CCodegen *cg);
void c_emit_target_postamble(CCodegen *cg);
int c_compile(CCodegen *cg, const char *c_path, const char *output_path);

/* Error handling codegen */
void c_emit_try(CCodegen *cg, AstNode *node);
void c_emit_throw(CCodegen *cg, AstNode *node);

#endif /* AETHER_C_TRANSPILER_H */
