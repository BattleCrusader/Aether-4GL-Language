#ifndef AETHER_SEMANTIC_H
#define AETHER_SEMANTIC_H

#include "defs.h"
#include "ast.h"
#include "arena.h"

/*
 * Semantic Analyzer — Phase 0 bootstrap version.
 * Handles: name resolution, basic type checking, scope management.
 * All results stored in AST nodes (resolved pointers, inferred types).
 */

typedef struct Scope Scope;

typedef struct {
    Arena *arena;
    Scope *global_scope;
    Scope *current_scope;
    int error_count;
} SemanticAnalyzer;

SemanticAnalyzer *semantic_create(Arena *a);
void semantic_destroy(SemanticAnalyzer *sa);
void semantic_analyze(SemanticAnalyzer *sa, AstNode *program);

/* Internal helpers exposed for testing */
void semantic_visit_node(SemanticAnalyzer *sa, AstNode *node);
void semantic_visit_func(SemanticAnalyzer *sa, AstNode *node);
void semantic_visit_block(SemanticAnalyzer *sa, AstNode *node);
void semantic_visit_expr(SemanticAnalyzer *sa, AstNode *node);

#endif /* AETHER_SEMANTIC_H */