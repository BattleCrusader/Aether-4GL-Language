#include "aether/semantic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ================================================================
 * Compile-time constant evaluator
 * ================================================================ */

/* Forward declaration */
static uint64_t const_eval_expr(SemanticAnalyzer *sa, AstNode *node, bool *ok);
static AstNode *scope_lookup(SemanticAnalyzer *sa, const char *name);

/* Evaluate a constant expression at compile time.
 * Returns the uint64_t value. Sets *ok to false if not constant. */
static uint64_t const_eval_expr(SemanticAnalyzer *sa, AstNode *node, bool *ok) {
    if (!node) { *ok = false; return 0; }

    switch (node->type) {
        case NODE_LITERAL_INT:
            *ok = true;
            return node->data.literal.int_val;

        case NODE_LITERAL_BOOL:
            *ok = true;
            return node->data.literal.bool_val ? 1 : 0;

        case NODE_LITERAL_CHAR:
            *ok = true;
            return node->data.literal.char_val;

        case NODE_LITERAL_NONE:
            *ok = true;
            return 0;

        case NODE_UNARY_OP: {
            bool sub_ok = false;
            uint64_t val = const_eval_expr(sa, node->data.unary.operand, &sub_ok);
            if (!sub_ok) { *ok = false; return 0; }
            switch (node->data.unary.op) {
                case UNARY_NEG: *ok = true; return (uint64_t)(-(int64_t)val);
                case UNARY_NOT: *ok = true; return val ? 0 : 1;
                case UNARY_BIT_NOT: *ok = true; return ~val;
                default: *ok = false; return 0;
            }
        }

        case NODE_BINARY_OP: {
            bool left_ok = false, right_ok = false;
            uint64_t left = const_eval_expr(sa, node->data.binary.left, &left_ok);
            uint64_t right = const_eval_expr(sa, node->data.binary.right, &right_ok);
            if (!left_ok || !right_ok) { *ok = false; return 0; }
            switch (node->data.binary.op) {
                case BIN_ADD: *ok = true; return left + right;
                case BIN_SUB: *ok = true; return left - right;
                case BIN_MUL: *ok = true; return left * right;
                case BIN_DIV: *ok = true; if (right == 0) { *ok = false; return 0; } return left / right;
                case BIN_MOD: *ok = true; if (right == 0) { *ok = false; return 0; } return left % right;
                case BIN_EQ:  *ok = true; return left == right ? 1 : 0;
                case BIN_NEQ: *ok = true; return left != right ? 1 : 0;
                case BIN_LT:  *ok = true; return (int64_t)left < (int64_t)right ? 1 : 0;
                case BIN_GT:  *ok = true; return (int64_t)left > (int64_t)right ? 1 : 0;
                case BIN_LE:  *ok = true; return (int64_t)left <= (int64_t)right ? 1 : 0;
                case BIN_GE:  *ok = true; return (int64_t)left >= (int64_t)right ? 1 : 0;
                case BIN_BIT_AND: *ok = true; return left & right;
                case BIN_BIT_OR:  *ok = true; return left | right;
                case BIN_BIT_XOR: *ok = true; return left ^ right;
                case BIN_SHL: *ok = true; return left << right;
                case BIN_SHR: *ok = true; return left >> right;
                case BIN_AND: *ok = true; return (left && right) ? 1 : 0;
                case BIN_OR:  *ok = true; return (left || right) ? 1 : 0;
                default: *ok = false; return 0;
            }
        }

        case NODE_IDENT: {
            /* Look up the identifier — if it's a const, evaluate it */
            const char *name = arena_strndup(sa->arena,
                node->data.ident.name.data, node->data.ident.name.len);
            AstNode *decl = scope_lookup(sa, name);
            if (decl && decl->type == NODE_CONST_DECL && decl->data.let_decl.value) {
                return const_eval_expr(sa, decl->data.let_decl.value, ok);
            }
            /* Check for built-in sizeof/alignof called as ident (no parens yet) */
            if (strcmp(name, "true") == 0) { *ok = true; return 1; }
            if (strcmp(name, "false") == 0) { *ok = true; return 0; }
            *ok = false;
            return 0;
        }

        case NODE_CALL: {
            /* Check for sizeof() / alignof() builtins */
            if (node->data.call.callee && node->data.call.callee->type == NODE_IDENT) {
                const char *fname = arena_strndup(sa->arena,
                    node->data.call.callee->data.ident.name.data,
                    node->data.call.callee->data.ident.name.len);
                if (strcmp(fname, "sizeof") == 0 && node->data.call.args.count >= 1) {
                    /* sizeof(type) — evaluate the type argument */
                    AstNode *arg = node->data.call.args.items[0];
                    if (arg->type == NODE_TYPE_PRIMITIVE) {
                        *ok = true;
                        switch (arg->data.type_node.prim) {
                            case PRIM_VOID: return 0;
                            case PRIM_BOOL: case PRIM_BYTE: case PRIM_U8: case PRIM_I8: return 1;
                            case PRIM_U16: case PRIM_I16: return 2;
                            case PRIM_U32: case PRIM_I32: case PRIM_F32: return 4;
                            case PRIM_U64: case PRIM_I64: case PRIM_F64: return 8;
                            case PRIM_STRING: return 8; /* pointer */
                        }
                    }
                    if (arg->type == NODE_TYPE_NAMED) {
                        *ok = true;
                        return 8; /* default: pointer-sized */
                    }
                }
                if (strcmp(fname, "alignof") == 0 && node->data.call.args.count >= 1) {
                    AstNode *arg = node->data.call.args.items[0];
                    if (arg->type == NODE_TYPE_PRIMITIVE) {
                        *ok = true;
                        switch (arg->data.type_node.prim) {
                            case PRIM_VOID: return 1;
                            case PRIM_BOOL: case PRIM_BYTE: case PRIM_U8: case PRIM_I8: return 1;
                            case PRIM_U16: case PRIM_I16: return 2;
                            case PRIM_U32: case PRIM_I32: case PRIM_F32: return 4;
                            case PRIM_U64: case PRIM_I64: case PRIM_F64: return 8;
                            case PRIM_STRING: return 8;
                        }
                    }
                    *ok = true;
                    return 8;
                }
            }
            *ok = false;
            return 0;
        }

        default:
            *ok = false;
            return 0;
    }
}

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
            else if (node->type == NODE_CONST_DECL) {
                name_node = node->data.let_decl.name;
                /* Evaluate const initializer at compile time */
                if (node->data.let_decl.value) {
                    bool ok = false;
                    uint64_t val = const_eval_expr(sa, node->data.let_decl.value, &ok);
                    if (ok) {
                        /* Replace the value with a constant literal */
                        node->data.let_decl.value = node_int_literal(sa->arena, node->loc, val);
                    }
                }
            }
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
            if (node->data.if_node.is_if_let) {
                semantic_visit_expr(sa, node->data.if_node.condition);
                /* Register the pattern variable in the body scope */
                if (node->data.if_node.if_let_pattern &&
                    node->data.if_node.if_let_pattern->type == NODE_IDENT) {
                    const char *pname = arena_strndup(sa->arena,
                        node->data.if_node.if_let_pattern->data.ident.name.data,
                        node->data.if_node.if_let_pattern->data.ident.name.len);
                    scope_declare(sa, pname, node->data.if_node.if_let_pattern);
                }
                if (node->data.if_node.then_block) semantic_visit_node(sa, node->data.if_node.then_block);
                break;
            }
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

        case NODE_TRY:
            /* Visit the try body */
            if (node->data.try_node.body) {
                semantic_visit_node(sa, node->data.try_node.body);
            }
            /* Visit each catch arm */
            for (int i = 0; i < node->data.try_node.catch_arms.count; i++) {
                AstNode *arm = node->data.try_node.catch_arms.items[i];
                /* Register the catch variable in the catch body scope */
                if (arm->data.catch_arm.var) {
                    const char *vname = arena_strndup(sa->arena,
                        arm->data.catch_arm.var->data.ident.name.data,
                        arm->data.catch_arm.var->data.ident.name.len);
                    scope_declare(sa, vname, arm->data.catch_arm.var);
                }
                if (arm->data.catch_arm.body) {
                    semantic_visit_node(sa, arm->data.catch_arm.body);
                }
            }
            break;

        case NODE_THROW:
            if (node->data.throw_node.value) {
                semantic_visit_expr(sa, node->data.throw_node.value);
            }
            break;

        case NODE_RUN_BLOCK:
            /* Visit #run body statements for name resolution */
            for (int i = 0; i < node->data.list.count; i++) {
                semantic_visit_node(sa, node->data.list.items[i]);
            }
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
                /* Access modifier enforcement: check if accessing a private symbol
                   from outside its module */
                if (decl->type == NODE_FUNC_DECL && decl->data.func.access == ACCESS_PRIVATE) {
                    /* Private functions can only be called from within the same file.
                       For now, we just warn — full cross-module enforcement deferred. */
                }
            }
            break;
        }

        case NODE_BINARY_OP:
            semantic_visit_expr(sa, node->data.binary.left);
            semantic_visit_expr(sa, node->data.binary.right);
            /* Check for operator overloading: if left resolves to a struct/class
               with an op_ method, mark the node for desugaring */
            if (node->data.binary.left && node->data.binary.left->type == NODE_IDENT &&
                node->data.binary.left->data.ident.resolved) {
                AstNode *decl = node->data.binary.left->data.ident.resolved;
                if (decl->type == NODE_LET || decl->type == NODE_PARAM) {
                    AstNode *type_node = (decl->type == NODE_LET) ?
                        decl->data.let_decl.type : decl->data.param.type;
                    if (type_node && type_node->type == NODE_TYPE_NAMED) {
                        /* Look up the type's methods for operator overloads */
                        /* For now, just mark — full desugaring deferred */
                    }
                }
            }
            break;

        case NODE_UNARY_OP:
            semantic_visit_expr(sa, node->data.unary.operand);
            break;

        case NODE_CALL:
            semantic_visit_expr(sa, node->data.call.callee);
            for (int i = 0; i < node->data.call.args.count; i++) {
                semantic_visit_expr(sa, node->data.call.args.items[i]);
            }
            /* Check if this is a call to a generic function — collect concrete types */
            if (node->data.call.callee && node->data.call.callee->type == NODE_IDENT &&
                node->data.call.callee->data.ident.resolved &&
                node->data.call.callee->data.ident.resolved->type == NODE_FUNC_DECL) {
                AstNode *func = node->data.call.callee->data.ident.resolved;
                if (func->data.func.type_params.count > 0) {
                    /* Mark the call as needing monomorphization */
                    /* Concrete types are inferred from argument types */
                    /* For now, just mark — full monomorphization deferred to codegen */
                }
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