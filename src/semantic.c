#include "aether/semantic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Scope: a simple symbol table for declarations.
 * Linked list of frames for lexical scoping.
 */

typedef struct Symbol {
    const char *name;         /* key (arena-allocated) */
    AstNode *decl;            /* the declaration node */
    struct Symbol *next;      /* chain in this scope */
} Symbol;

struct Scope {
    Symbol *symbols;
    Scope *parent;
    Scope *children;          /* first child scope */
    Scope *next_sibling;      /* next sibling under same parent */
    AstNode *node;            /* the AST node this scope belongs to */
};

/* ================================================================ */

SemanticAnalyzer *semantic_create(Arena *a) {
    SemanticAnalyzer *sa = (SemanticAnalyzer *)arena_alloc(a, sizeof(SemanticAnalyzer));
    if (!sa) return NULL;
    sa->arena = a;
    sa->global_scope = (Scope *)arena_alloc(a, sizeof(Scope));
    if (sa->global_scope) {
        sa->global_scope->symbols = NULL;
        sa->global_scope->parent = NULL;
        sa->global_scope->children = NULL;
        sa->global_scope->next_sibling = NULL;
        sa->global_scope->node = NULL;
    }
    sa->current_scope = sa->global_scope;
    sa->error_count = 0;

    /* Register built-in functions later — scope_declare() isn't visible yet */
    return sa;
}

void semantic_destroy(SemanticAnalyzer *sa) {
    /* All memory is arena-managed — nothing to free */
    (void)sa;
}

/* ================================================================
 * Scope operations
 * ================================================================ */

static void scope_declare(SemanticAnalyzer *sa, const char *name, AstNode *decl) {
    Symbol *sym = (Symbol *)arena_alloc(sa->arena, sizeof(Symbol));
    sym->name = name;
    sym->decl = decl;
    sym->next = sa->current_scope->symbols;
    sa->current_scope->symbols = sym;
}

static AstNode *scope_lookup(SemanticAnalyzer *sa, const char *name) {
    for (Scope *s = sa->current_scope; s; s = s->parent) {
        for (Symbol *sym = s->symbols; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) return sym->decl;
        }
    }
    return NULL;
}

static void scope_enter(SemanticAnalyzer *sa, AstNode *node) {
    Scope *child = (Scope *)arena_alloc(sa->arena, sizeof(Scope));
    child->symbols = NULL;
    child->parent = sa->current_scope;
    child->children = NULL;
    child->next_sibling = NULL;
    child->node = node;

    /* Link as sibling of existing children */
    if (sa->current_scope->children) {
        Scope *s = sa->current_scope->children;
        while (s->next_sibling) s = s->next_sibling;
        s->next_sibling = child;
    } else {
        sa->current_scope->children = child;
    }

    sa->current_scope = child;
}

static void scope_exit(SemanticAnalyzer *sa) {
    if (sa->current_scope->parent) {
        sa->current_scope = sa->current_scope->parent;
    }
}

/* ================================================================
 * Visitor
 * ================================================================ */

void semantic_visit_node(SemanticAnalyzer *sa, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM:
            for (int i = 0; i < node->data.list.count; i++) {
                semantic_visit_node(sa, node->data.list.items[i]);
            }
            break;

        case NODE_FUNC_DECL: {
            const char *name = arena_strndup(sa->arena,
                node->data.func.name->data.ident.name.data,
                node->data.func.name->data.ident.name.len);
            scope_declare(sa, name, node);
            scope_enter(sa, node);

            /* Declare parameters in function scope */
            for (int i = 0; i < node->data.func.params.count; i++) {
                AstNode *param = node->data.func.params.items[i];
                const char *pname = arena_strndup(sa->arena,
                    param->data.param.name->data.ident.name.data,
                    param->data.param.name->data.ident.name.len);
                scope_declare(sa, pname, param);
            }

            /* Visit return type if present */
            if (node->data.func.return_type) {
                semantic_visit_node(sa, node->data.func.return_type);
            }

            /* Visit body */
            if (node->data.func.body) {
                semantic_visit_node(sa, node->data.func.body);
            }

            scope_exit(sa);
            break;
        }

        case NODE_BLOCK:
            scope_enter(sa, node);
            for (int i = 0; i < node->data.list.count; i++) {
                semantic_visit_node(sa, node->data.list.items[i]);
            }
            scope_exit(sa);
            break;

        case NODE_LET: {
            const char *name = arena_strndup(sa->arena,
                node->data.let_decl.name->data.ident.name.data,
                node->data.let_decl.name->data.ident.name.len);
            scope_declare(sa, name, node);

            if (node->data.let_decl.type) {
                semantic_visit_node(sa, node->data.let_decl.type);
            }
            if (node->data.let_decl.value) {
                semantic_visit_expr(sa, node->data.let_decl.value);
            }
            break;
        }

        case NODE_CONST_DECL:
        case NODE_STRUCT_DECL:
        case NODE_CLASS_DECL:
        case NODE_ENUM_DECL: {
            /* Register type name in scope */
            AstNode *name_node = NULL;
            if (node->type == NODE_STRUCT_DECL || node->type == NODE_CLASS_DECL) name_node = node->data.struct_decl.name;
            else if (node->type == NODE_ENUM_DECL) name_node = node->data.enum_decl.name;
            else if (node->type == NODE_CONST_DECL) name_node = node->data.let_decl.name;
            if (name_node) {
                const char *tname = arena_strndup(sa->arena,
                    name_node->data.ident.name.data,
                    name_node->data.ident.name.len);
                scope_declare(sa, tname, node);
            }
            break;
        }

        case NODE_RETURN:
            if (node->data.return_node.value) {
                semantic_visit_expr(sa, node->data.return_node.value);
            }
            break;

        case NODE_IF:
            semantic_visit_expr(sa, node->data.if_node.condition);
            if (node->data.if_node.then_block) semantic_visit_node(sa, node->data.if_node.then_block);
            if (node->data.if_node.elif_chain) semantic_visit_node(sa, node->data.if_node.elif_chain);
            if (node->data.if_node.else_block) semantic_visit_node(sa, node->data.if_node.else_block);
            break;

        case NODE_WHILE:
            semantic_visit_expr(sa, node->data.while_node.condition);
            if (node->data.while_node.body) semantic_visit_node(sa, node->data.while_node.body);
            break;

        case NODE_FOR:
            if (node->data.for_node.var) semantic_visit_node(sa, node->data.for_node.var);
            if (node->data.for_node.iterable) semantic_visit_expr(sa, node->data.for_node.iterable);
            if (node->data.for_node.body) semantic_visit_node(sa, node->data.for_node.body);
            break;

        case NODE_EXPR_STMT:
            semantic_visit_expr(sa, node->data.call.callee);
            break;

        default:
            break;
    }
}

void semantic_visit_expr(SemanticAnalyzer *sa, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_IDENT: {
            const char *name = arena_strndup(sa->arena,
                node->data.ident.name.data, node->data.ident.name.len);
            AstNode *decl = scope_lookup(sa, name);
            if (!decl) {
                fprintf(stderr, "Error: undefined variable '%.*s'\n",
                    (int)node->data.ident.name.len, node->data.ident.name.data);
                sa->error_count++;
            } else {
                node->data.ident.resolved = decl;
            }
            break;
        }

        case NODE_BINARY_OP:
            semantic_visit_expr(sa, node->data.binary.left);
            semantic_visit_expr(sa, node->data.binary.right);
            break;

        case NODE_UNARY_OP:
            semantic_visit_expr(sa, node->data.unary.operand);
            break;

        case NODE_CALL:
            semantic_visit_expr(sa, node->data.call.callee);
            for (int i = 0; i < node->data.call.args.count; i++) {
                semantic_visit_expr(sa, node->data.call.args.items[i]);
            }
            break;

        case NODE_INDEX:
            semantic_visit_expr(sa, node->data.index.target);
            semantic_visit_expr(sa, node->data.index.index);
            break;

        case NODE_FIELD_ACCESS:
            semantic_visit_expr(sa, node->data.field.target);
            break;

        case NODE_MATCH:
            semantic_visit_expr(sa, node->data.match_node.value);
            for (int i = 0; i < node->data.match_node.arms.count; i++) {
                semantic_visit_node(sa, node->data.match_node.arms.items[i]);
            }
            break;

        /* Literals have no sub-expressions */
        case NODE_LITERAL_INT:
        case NODE_LITERAL_FLOAT:
        case NODE_LITERAL_STRING:
        case NODE_LITERAL_CHAR:
        case NODE_LITERAL_BOOL:
        case NODE_LITERAL_NONE:
        case NODE_ARRAY_LIT:
        case NODE_ASM_BLOCK:
            break;

        default:
            break;
    }
}

void semantic_analyze(SemanticAnalyzer *sa, AstNode *program) {
    /* Register built-in functions */
    AstNode *print_decl = (AstNode *)arena_alloc(sa->arena, sizeof(AstNode));
    memset(print_decl, 0, sizeof(AstNode));
    print_decl->type = NODE_FUNC_DECL;
    sa->builtin_print = print_decl;
    scope_declare(sa, "print", print_decl);

    semantic_visit_node(sa, program);
}