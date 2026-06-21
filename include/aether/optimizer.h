#ifndef AETHER_OPTIMIZER_H
#define AETHER_OPTIMIZER_H

#include "ast.h"
#include "arena.h"

/*
 * Optimization passes for the Aether compiler.
 * All passes operate on the AST and can be enabled/disabled independently.
 */

typedef struct {
    bool constant_folding;      /* P09.01 */
    bool dead_code_elimination; /* P09.02 */
    bool inlining;              /* P09.03 */
    bool escape_analysis;       /* P09.04 */
    bool region_elision;        /* P09.05 */
    bool devirtualization;      /* P09.06 */
    bool loop_unrolling;        /* P09.07 */
    bool memory_fusion;         /* P09.08 */
    bool verbose;               /* print optimization stats */
} OptimizerConfig;

/* Default config: all optimizations enabled */
void optimizer_config_init(OptimizerConfig *cfg);

/* Run all enabled optimization passes on the AST.
 * Returns the (possibly modified) AST root.
 * The AST is modified in-place; no deep copy is made. */
AstNode *optimize(AstNode *program, Arena *arena, const OptimizerConfig *cfg);

/* ================================================================
 * Individual optimization passes (can be called independently)
 * ================================================================ */

/* P09.01 — Constant folding and propagation.
 * Evaluates constant sub-expressions at compile time.
 * Replaces 3+4 with 7, propagates constant let bindings. */
AstNode *opt_constant_fold(AstNode *node, Arena *arena);

/* P09.02 — Dead code elimination.
 * Removes unreachable branches, dead variables, dead functions. */
AstNode *opt_dead_code_elim(AstNode *node, Arena *arena);

/* P09.03 — Aggressive inlining.
 * Inlines small functions and @force_inline marked functions. */
AstNode *opt_inline(AstNode *node, Arena *arena);

/* P09.04 — Escape analysis.
 * Promotes heap allocations to stack when they don't escape. */
AstNode *opt_escape_analysis(AstNode *node, Arena *arena);

/* P09.05 — Region inference / arena elision.
 * Elides unnecessary region allocations for small regions. */
AstNode *opt_region_elision(AstNode *node, Arena *arena);

/* P09.06 — Devirtualization.
 * Converts dyn Trait calls to static dispatch when type is known. */
AstNode *opt_devirtualize(AstNode *node, Arena *arena);

/* P09.07 — Loop unrolling.
 * Unrolls small fixed-count loops. */
AstNode *opt_loop_unroll(AstNode *node, Arena *arena);

/* P09.08 — Memory operation fusion.
 * Fuses adjacent loads/stores into wider operations. */
AstNode *opt_memory_fusion(AstNode *node, Arena *arena);

/* ================================================================
 * Utility functions for optimization passes
 * ================================================================ */

/* Check if an expression is a compile-time constant */
bool is_constant_expr(AstNode *node);

/* Evaluate a constant expression to an integer (if possible).
 * Returns true and sets *result on success. */
bool eval_constant_int(AstNode *node, uint64_t *result);

/* Check if a function body is "small" enough to inline.
 * "Small" = fewer than N AST nodes in the body. */
bool is_small_function(AstNode *func_decl, int max_nodes);

/* Count AST nodes in a subtree (for inlining heuristics) */
int count_ast_nodes(AstNode *node);

/* Check if a variable is used after a given point in the AST */
bool is_var_used_after(AstNode *scope, const char *var_name, AstNode *after_point);

/* Check if a value escapes a function (is returned, stored in global, or passed to unknown func) */
bool does_value_escape(AstNode *func_decl, AstNode *value);

#endif /* AETHER_OPTIMIZER_H */
