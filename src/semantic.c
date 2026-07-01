#include "aether/semantic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ================================================================
 * Generics monomorphization
 * ================================================================ */

/* Deep-clone a type node, substituting generic type params with concrete types.
 * type_params: list of NODE_IDENT for the generic param names.
 * concrete_types: list of NODE_TYPE_* nodes to substitute.
 * Returns a new arena-allocated type node. */
static AstNode *monomorphize_type(Arena *arena, AstNode *type_node,
                                   AstNodeList *type_params, AstNodeList *concrete_types) {
    if (!type_node) return NULL;
    switch (type_node->type) {
        case NODE_TYPE_PRIMITIVE:
            return node_type_prim(arena, type_node->loc, type_node->data.type_node.prim);
        case NODE_TYPE_NAMED: {
            /* Check if this named type matches a generic param */
            for (int i = 0; i < type_params->count; i++) {
                AstNode *tp = type_params->items[i];
                if (tp->type == NODE_IDENT && type_node->data.type_node.name.len == tp->data.ident.name.len &&
                    memcmp(type_node->data.type_node.name.data, tp->data.ident.name.data, type_node->data.type_node.name.len) == 0) {
                    /* Substitute with concrete type */
                    if (i < concrete_types->count) {
                        return monomorphize_type(arena, concrete_types->items[i], type_params, concrete_types);
                    }
                }
            }
            return node_type_named(arena, type_node->loc, type_node->data.type_node.name);
        }
        case NODE_TYPE_REF: {
            AstNode *t = node_create(arena, NODE_TYPE_REF, type_node->loc);
            t->data.type_node.elem_type = monomorphize_type(arena, type_node->data.type_node.elem_type, type_params, concrete_types);
            t->data.type_node.is_ref = type_node->data.type_node.is_ref;
            return t;
        }
        case NODE_TYPE_PTR: {
            AstNode *t = node_create(arena, NODE_TYPE_PTR, type_node->loc);
            t->data.type_node.elem_type = monomorphize_type(arena, type_node->data.type_node.elem_type, type_params, concrete_types);
            return t;
        }
        case NODE_TYPE_ARRAY: {
            AstNode *t = node_create(arena, NODE_TYPE_ARRAY, type_node->loc);
            t->data.type_node.elem_type = monomorphize_type(arena, type_node->data.type_node.elem_type, type_params, concrete_types);
            t->data.type_node.array_size = type_node->data.type_node.array_size;
            return t;
        }
        case NODE_TYPE_SLICE: {
            AstNode *t = node_create(arena, NODE_TYPE_SLICE, type_node->loc);
            t->data.type_node.elem_type = monomorphize_type(arena, type_node->data.type_node.elem_type, type_params, concrete_types);
            return t;
        }
        case NODE_TYPE_OPTIONAL: {
            AstNode *t = node_create(arena, NODE_TYPE_OPTIONAL, type_node->loc);
            t->data.type_node.elem_type = monomorphize_type(arena, type_node->data.type_node.elem_type, type_params, concrete_types);
            return t;
        }
        case NODE_TYPE_TUPLE: {
            AstNode *t = node_create(arena, NODE_TYPE_TUPLE, type_node->loc);
            for (int i = 0; i < type_node->data.type_node.param_types.count; i++) {
                node_list_append(&t->data.type_node.param_types,
                    monomorphize_type(arena, type_node->data.type_node.param_types.items[i], type_params, concrete_types));
            }
            return t;
        }
        case NODE_TYPE_FN: {
            AstNode *t = node_create(arena, NODE_TYPE_FN, type_node->loc);
            for (int i = 0; i < type_node->data.type_node.param_types.count; i++) {
                node_list_append(&t->data.type_node.param_types,
                    monomorphize_type(arena, type_node->data.type_node.param_types.items[i], type_params, concrete_types));
            }
            t->data.type_node.return_type = monomorphize_type(arena, type_node->data.type_node.return_type, type_params, concrete_types);
            return t;
        }
        default:
            return type_node; /* pass through for non-type nodes */
    }
}

/* Deep-clone an expression node, substituting generic type params with concrete types. */
static AstNode *monomorphize_expr(Arena *arena, AstNode *node,
                                   AstNodeList *type_params, AstNodeList *concrete_types) {
    if (!node) return NULL;
    AstNode *copy = node_create(arena, node->type, node->loc);
    switch (node->type) {
        case NODE_LITERAL_INT:
            copy->data.literal.int_val = node->data.literal.int_val;
            break;
        case NODE_LITERAL_FLOAT:
            copy->data.literal.float_val = node->data.literal.float_val;
            break;
        case NODE_LITERAL_STRING:
            copy->data.literal.string_val = node->data.literal.string_val;
            break;
        case NODE_LITERAL_CHAR:
            copy->data.literal.char_val = node->data.literal.char_val;
            break;
        case NODE_LITERAL_BOOL:
            copy->data.literal.bool_val = node->data.literal.bool_val;
            break;
        case NODE_LITERAL_NONE:
            break;
        case NODE_IDENT:
            copy->data.ident.name = node->data.ident.name;
            copy->data.ident.resolved = node->data.ident.resolved;
            break;
        case NODE_BINARY_OP:
            copy->data.binary.op = node->data.binary.op;
            copy->data.binary.left = monomorphize_expr(arena, node->data.binary.left, type_params, concrete_types);
            copy->data.binary.right = monomorphize_expr(arena, node->data.binary.right, type_params, concrete_types);
            break;
        case NODE_UNARY_OP:
            copy->data.unary.op = node->data.unary.op;
            copy->data.unary.operand = monomorphize_expr(arena, node->data.unary.operand, type_params, concrete_types);
            break;
        case NODE_CALL:
            copy->data.call.callee = monomorphize_expr(arena, node->data.call.callee, type_params, concrete_types);
            for (int i = 0; i < node->data.call.args.count; i++) {
                node_list_append(&copy->data.call.args,
                    monomorphize_expr(arena, node->data.call.args.items[i], type_params, concrete_types));
            }
            break;
        case NODE_INDEX:
            copy->data.index.target = monomorphize_expr(arena, node->data.index.target, type_params, concrete_types);
            copy->data.index.index = monomorphize_expr(arena, node->data.index.index, type_params, concrete_types);
            break;
        case NODE_FIELD_ACCESS:
            copy->data.field.target = monomorphize_expr(arena, node->data.field.target, type_params, concrete_types);
            copy->data.field.field = monomorphize_expr(arena, node->data.field.field, type_params, concrete_types);
            break;
        case NODE_CAST:
            copy->data.binary.left = monomorphize_expr(arena, node->data.binary.left, type_params, concrete_types);
            copy->data.binary.right = monomorphize_type(arena, node->data.binary.right, type_params, concrete_types);
            break;
        case NODE_ARRAY_LIT:
            for (int i = 0; i < node->data.list.count; i++) {
                node_list_append(&copy->data.list,
                    monomorphize_expr(arena, node->data.list.items[i], type_params, concrete_types));
            }
            break;
        default:
            break;
    }
    return copy;
}

/* Deep-clone a statement/declaration node, substituting generic type params with concrete types. */
static AstNode *monomorphize_node(Arena *arena, AstNode *node,
                                   AstNodeList *type_params, AstNodeList *concrete_types) {
    if (!node) return NULL;
    AstNode *copy = node_create(arena, node->type, node->loc);
    switch (node->type) {
        case NODE_BLOCK:
            for (int i = 0; i < node->data.list.count; i++) {
                node_list_append(&copy->data.list,
                    monomorphize_node(arena, node->data.list.items[i], type_params, concrete_types));
            }
            break;
        case NODE_LET:
            copy->data.let_decl.name = monomorphize_expr(arena, node->data.let_decl.name, type_params, concrete_types);
            copy->data.let_decl.type = monomorphize_type(arena, node->data.let_decl.type, type_params, concrete_types);
            copy->data.let_decl.value = monomorphize_expr(arena, node->data.let_decl.value, type_params, concrete_types);
            copy->data.let_decl.is_mut = node->data.let_decl.is_mut;
            copy->data.let_decl.is_global = node->data.let_decl.is_global;
            break;
        case NODE_RETURN:
            copy->data.return_node.value = monomorphize_expr(arena, node->data.return_node.value, type_params, concrete_types);
            break;
        case NODE_IF:
            copy->data.if_node.condition = monomorphize_expr(arena, node->data.if_node.condition, type_params, concrete_types);
            copy->data.if_node.then_block = monomorphize_node(arena, node->data.if_node.then_block, type_params, concrete_types);
            copy->data.if_node.elif_chain = monomorphize_node(arena, node->data.if_node.elif_chain, type_params, concrete_types);
            copy->data.if_node.else_block = monomorphize_node(arena, node->data.if_node.else_block, type_params, concrete_types);
            break;
        case NODE_WHILE:
            copy->data.while_node.condition = monomorphize_expr(arena, node->data.while_node.condition, type_params, concrete_types);
            copy->data.while_node.body = monomorphize_node(arena, node->data.while_node.body, type_params, concrete_types);
            break;
        case NODE_FOR:
            copy->data.for_node.var = monomorphize_expr(arena, node->data.for_node.var, type_params, concrete_types);
            copy->data.for_node.index_var = monomorphize_expr(arena, node->data.for_node.index_var, type_params, concrete_types);
            copy->data.for_node.iterable = monomorphize_expr(arena, node->data.for_node.iterable, type_params, concrete_types);
            copy->data.for_node.body = monomorphize_node(arena, node->data.for_node.body, type_params, concrete_types);
            break;
        case NODE_EXPR_STMT:
            copy->data.call.callee = monomorphize_expr(arena, node->data.call.callee, type_params, concrete_types);
            break;
        case NODE_MATCH:
            copy->data.match_node.value = monomorphize_expr(arena, node->data.match_node.value, type_params, concrete_types);
            for (int i = 0; i < node->data.match_node.arms.count; i++) {
                node_list_append(&copy->data.match_node.arms,
                    monomorphize_node(arena, node->data.match_node.arms.items[i], type_params, concrete_types));
            }
            break;
        case NODE_MATCH_ARM:
            copy->data.match_arm.pattern = monomorphize_expr(arena, node->data.match_arm.pattern, type_params, concrete_types);
            copy->data.match_arm.body = monomorphize_node(arena, node->data.match_arm.body, type_params, concrete_types);
            break;
        case NODE_TRY:
            copy->data.try_node.body = monomorphize_node(arena, node->data.try_node.body, type_params, concrete_types);
            for (int i = 0; i < node->data.try_node.catch_arms.count; i++) {
                node_list_append(&copy->data.try_node.catch_arms,
                    monomorphize_node(arena, node->data.try_node.catch_arms.items[i], type_params, concrete_types));
            }
            copy->data.try_node.finally_body = monomorphize_node(arena, node->data.try_node.finally_body, type_params, concrete_types);
            break;
        case NODE_CATCH_ARM:
            copy->data.catch_arm.var = monomorphize_expr(arena, node->data.catch_arm.var, type_params, concrete_types);
            copy->data.catch_arm.body = monomorphize_node(arena, node->data.catch_arm.body, type_params, concrete_types);
            break;
        case NODE_THROW:
            copy->data.throw_node.value = monomorphize_expr(arena, node->data.throw_node.value, type_params, concrete_types);
            break;
        case NODE_DEFER:
            copy->data.defer_node.body = monomorphize_node(arena, node->data.defer_node.body, type_params, concrete_types);
            break;
        case NODE_ASM_BLOCK:
            copy->data.asm_block.text = node->data.asm_block.text;
            copy->data.asm_block.outputs = node->data.asm_block.outputs;
            copy->data.asm_block.inputs = node->data.asm_block.inputs;
            copy->data.asm_block.clobbers = node->data.asm_block.clobbers;
            break;
        default:
            break;
    }
    return copy;
}

/* Create a monomorphized copy of a generic function with concrete types substituted.
 * The copy is added to the program's top-level declarations. */
static AstNode *monomorphize_func(Arena *arena, AstNode *program, AstNode *generic_func,
                                   AstNodeList *concrete_types) {
    AstNodeList *type_params = &generic_func->data.func.type_params;
    if (type_params->count == 0) return generic_func;

    /* Build a mangled name: funcName_T_U where T, U are the concrete type names */
    char mangled_name[256];
    int pos = 0;
    StringView fname = generic_func->data.func.name->data.ident.name;
    if (pos + (int)fname.len < 256) {
        memcpy(mangled_name + pos, fname.data, fname.len);
        pos += fname.len;
    }
    for (int i = 0; i < concrete_types->count && pos < 250; i++) {
        mangled_name[pos++] = '_';
        AstNode *ct = concrete_types->items[i];
        if (ct->type == NODE_TYPE_PRIMITIVE) {
            const char *pn = NULL;
            switch (ct->data.type_node.prim) {
                case PRIM_VOID: pn = "void"; break;
                case PRIM_BOOL: pn = "bool"; break;
                case PRIM_BYTE: case PRIM_U8: pn = "u8"; break;
                case PRIM_U16: pn = "u16"; break;
                case PRIM_U32: pn = "u32"; break;
                case PRIM_U64: pn = "u64"; break;
                case PRIM_U128: pn = "u128"; break;
                case PRIM_I8: pn = "i8"; break;
                case PRIM_I16: pn = "i16"; break;
                case PRIM_I32: pn = "i32"; break;
                case PRIM_I64: pn = "i64"; break;
                case PRIM_F32: pn = "f32"; break;
                case PRIM_F64: pn = "f64"; break;
                case PRIM_STRING: pn = "string"; break;
                default: pn = "unknown"; break;
            }
            int plen = strlen(pn);
            if (pos + plen < 256) {
                memcpy(mangled_name + pos, pn, plen);
                pos += plen;
            }
        } else if (ct->type == NODE_TYPE_NAMED) {
            int nlen = (int)ct->data.type_node.name.len;
            if (pos + nlen < 256) {
                memcpy(mangled_name + pos, ct->data.type_node.name.data, nlen);
                pos += nlen;
            }
        }
    }
    mangled_name[pos] = '\0';

    /* Create the monomorphized function declaration */
    AstNode *name_node = node_ident(arena, generic_func->data.func.name->loc,
        (StringView){mangled_name, (size_t)pos});
    AstNode *mono = node_func_decl(arena, generic_func->loc, name_node,
        generic_func->data.func.is_pub, generic_func->data.func.is_static);
    mono->data.func.is_asm = generic_func->data.func.is_asm;
    mono->data.func.is_sys = generic_func->data.func.is_sys;
    mono->data.func.is_throws = generic_func->data.func.is_throws;
    mono->data.func.is_operator = generic_func->data.func.is_operator;
    mono->data.func.is_inline = generic_func->data.func.is_inline;
    mono->data.func.is_force_inline = generic_func->data.func.is_force_inline;
    mono->data.func.is_no_inline = generic_func->data.func.is_no_inline;
    mono->data.func.access = generic_func->data.func.access;

    /* Monomorphize return type */
    mono->data.func.return_type = monomorphize_type(arena, generic_func->data.func.return_type, type_params, concrete_types);

    /* Monomorphize parameters */
    for (int i = 0; i < generic_func->data.func.params.count; i++) {
        AstNode *param = generic_func->data.func.params.items[i];
        AstNode *pname = monomorphize_expr(arena, param->data.param.name, type_params, concrete_types);
        AstNode *ptype = monomorphize_type(arena, param->data.param.type, type_params, concrete_types);
        AstNode *new_param = node_create(arena, NODE_PARAM, param->loc);
        new_param->data.param.name = pname;
        new_param->data.param.type = ptype;
        new_param->data.param.is_varargs = param->data.param.is_varargs;
        node_list_append(&mono->data.func.params, new_param);
    }

    /* Monomorphize body */
    if (generic_func->data.func.body) {
        mono->data.func.body = monomorphize_node(arena, generic_func->data.func.body, type_params, concrete_types);
    }

    /* Add to program */
    node_list_append(&program->data.list, mono);

    return mono;
}

/* Forward declaration */
static uint64_t const_eval_expr(SemanticAnalyzer *sa, AstNode *node, bool *ok);
static AstNode *scope_lookup(SemanticAnalyzer *sa, const char *name);
static void scope_declare(SemanticAnalyzer *sa, const char *name, AstNode *decl);

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
            /* First pass: declare all top-level names */
            for (int i = 0; i < node->data.list.count; i++) {
                AstNode *decl = node->data.list.items[i];
                if (decl->type == NODE_FUNC_DECL || decl->type == NODE_PROPERTY) {
                    const char *name = arena_strndup(sa->arena,
                        decl->data.func.name->data.ident.name.data,
                        decl->data.func.name->data.ident.name.len);
                    fprintf(stderr, "  FIRST PASS: declaring func '%s' (decl at %p)\n", name, (void*)decl);
                    scope_declare(sa, name, decl);
                } else if (decl->type == NODE_CONST_DECL) {
                    const char *name = arena_strndup(sa->arena,
                        decl->data.let_decl.name->data.ident.name.data,
                        decl->data.let_decl.name->data.ident.name.len);
                    scope_declare(sa, name, decl);
                } else if (decl->type == NODE_LET && decl->data.let_decl.is_global) {
                    const char *name = arena_strndup(sa->arena,
                        decl->data.let_decl.name->data.ident.name.data,
                        decl->data.let_decl.name->data.ident.name.len);
                    scope_declare(sa, name, decl);
                } else if (decl->type == NODE_STRUCT_DECL) {
                    const char *name = arena_strndup(sa->arena,
                        decl->data.struct_decl.name->data.ident.name.data,
                        decl->data.struct_decl.name->data.ident.name.len);
                    scope_declare(sa, name, decl);
                } else if (decl->type == NODE_ENUM_DECL) {
                    const char *name = arena_strndup(sa->arena,
                        decl->data.enum_decl.name->data.ident.name.data,
                        decl->data.enum_decl.name->data.ident.name.len);
                    scope_declare(sa, name, decl);
                } else if (decl->type == NODE_TRAIT_DECL) {
                    const char *name = arena_strndup(sa->arena,
                        decl->data.trait_decl.name->data.ident.name.data,
                        decl->data.trait_decl.name->data.ident.name.len);
                    scope_declare(sa, name, decl);
                } else if (decl->type == NODE_CLASS_DECL) {
                    const char *name = arena_strndup(sa->arena,
                        decl->data.struct_decl.name->data.ident.name.data,
                        decl->data.struct_decl.name->data.ident.name.len);
                    scope_declare(sa, name, decl);
                }
            }
            /* Second pass: visit bodies (func decls also declare their names again, but that's fine) */
            for (int i = 0; i < node->data.list.count; i++) {
                AstNode *decl = node->data.list.items[i];
                /* Skip IMPORT nodes — they should have been resolved already */
                if (decl->type == NODE_IMPORT) continue;
                semantic_visit_node(sa, decl);
            }
            break;

        case NODE_FUNC_DECL:
        case NODE_PROPERTY: {
            const char *name = arena_strndup(sa->arena,
                node->data.func.name->data.ident.name.data,
                node->data.func.name->data.ident.name.len);
            scope_declare(sa, name, node);
            scope_enter(sa, node);

            /* Declare generic type parameters in function scope */
            for (int i = 0; i < node->data.func.type_params.count; i++) {
                AstNode *tp = node->data.func.type_params.items[i];
                if (tp->type == NODE_IDENT) {
                    const char *tp_name = arena_strndup(sa->arena,
                        tp->data.ident.name.data, tp->data.ident.name.len);
                    scope_declare(sa, tp_name, tp);
                }
            }

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

            /* Check: if function has a non-void return type, body must end with a return */
            if (node->data.func.return_type && node->data.func.body) {
                /* Skip check for void functions */
                if (!(node->data.func.return_type->type == NODE_TYPE_PRIMITIVE &&
                    node->data.func.return_type->data.type_node.prim == PRIM_VOID)) {
                    bool has_return = false;
                    if (node->data.func.body->type == NODE_RETURN) {
                        has_return = true;
                    } else if (node->data.func.body->type == NODE_ASM_BLOCK) {
                        /* asm block body counts as having a return (compiler adds epilogue) */
                        has_return = true;
                    } else if (node->data.func.body->type == NODE_BLOCK) {
                        AstNodeList *stmts = &node->data.func.body->data.list;
                        if (stmts->count > 0) {
                            AstNode *last = stmts->items[stmts->count - 1];
                            if (last->type == NODE_RETURN) {
                                has_return = true;
                            }
                            /* asm block counts as having a return (compiler adds epilogue) */
                            if (last->type == NODE_ASM_BLOCK) {
                                has_return = true;
                            }
                        }
                    }
                    if (!has_return) {
                        fprintf(stderr, "Error: function '%.*s' has return type but no return statement\n",
                            (int)node->data.func.name->data.ident.name.len,
                            node->data.func.name->data.ident.name.data);
                        sa->error_count++;
                    }
                }
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

            /* Visit value BEFORE declaring the name, so the initializer
               can reference the function with the same name (e.g. let test = test()) */
            if (node->data.let_decl.value) {
                semantic_visit_expr(sa, node->data.let_decl.value);
            }

            scope_declare(sa, name, node);

            if (node->data.let_decl.type) {
                semantic_visit_node(sa, node->data.let_decl.type);
            }
            break;
        }

        case NODE_CONST_DECL:
        case NODE_TYPE_ALIAS:
        case NODE_STRUCT_DECL:
        case NODE_CLASS_DECL:
        case NODE_ENUM_DECL: {
            /* Register type name in scope */
            AstNode *name_node = NULL;
            if (node->type == NODE_STRUCT_DECL || node->type == NODE_CLASS_DECL) name_node = node->data.struct_decl.name;
            else if (node->type == NODE_ENUM_DECL) name_node = node->data.enum_decl.name;
            else if (node->type == NODE_TYPE_ALIAS) {
                /* Type alias: name is first item in list */
                if (node->data.list.count > 0) name_node = node->data.list.items[0];
            }
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
            /* Visit struct/class methods */
            if (node->type == NODE_STRUCT_DECL || node->type == NODE_CLASS_DECL) {
                for (int mi = 0; mi < node->data.struct_decl.methods.count; mi++) {
                    semantic_visit_node(sa, node->data.struct_decl.methods.items[mi]);
                }
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
            /* Declare the loop variable in scope */
            if (node->data.for_node.var) {
                const char *vname = arena_strndup(sa->arena,
                    node->data.for_node.var->data.ident.name.data,
                    node->data.for_node.var->data.ident.name.len);
                scope_declare(sa, vname, node->data.for_node.var);
                semantic_visit_node(sa, node->data.for_node.var);
            }
            /* Also declare index variable if present */
            if (node->data.for_node.index_var) {
                AstNode *index_var = node->data.for_node.index_var;
                if (index_var) {
                    const char *iname = arena_strndup(sa->arena,
                        index_var->data.ident.name.data,
                        index_var->data.ident.name.len);
                    scope_declare(sa, iname, index_var);
                    semantic_visit_node(sa, index_var);
                }
            }
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
            /* Visit finally block */
            if (node->data.try_node.finally_body) {
                semantic_visit_node(sa, node->data.try_node.finally_body);
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
            /* BIN_CUSTOM: custom operator (unicode symbol) — no special analysis needed */
            if (node->data.binary.op == BIN_CUSTOM) {
                break;
            }
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
            /* Constructor pattern: TypeName(expr) — skip resolution for primitive type names */
            if (node->data.call.callee && node->data.call.callee->type == NODE_IDENT &&
                node->data.call.args.count == 1) {
                const char *cn = arena_strndup(sa->arena,
                    node->data.call.callee->data.ident.name.data,
                    node->data.call.callee->data.ident.name.len);
                if (strcmp(cn, "string") == 0 ||
                    strcmp(cn, "u8") == 0 || strcmp(cn, "u16") == 0 ||
                    strcmp(cn, "u32") == 0 || strcmp(cn, "u64") == 0 ||
                    strcmp(cn, "i8") == 0 || strcmp(cn, "i16") == 0 ||
                    strcmp(cn, "i32") == 0 || strcmp(cn, "i64") == 0 ||
                    strcmp(cn, "f32") == 0 || strcmp(cn, "f64") == 0 ||
                    strcmp(cn, "bool") == 0 || strcmp(cn, "byte") == 0) {
                    /* Constructor call — just visit the argument, don't resolve callee */
                    for (int i = 0; i < node->data.call.args.count; i++) {
                        semantic_visit_expr(sa, node->data.call.args.items[i]);
                    }
                    break;
                }
            }
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
                    /* Infer concrete types from argument types */
                    AstNodeList concrete_types = {0};
                    for (int i = 0; i < func->data.func.type_params.count; i++) {
                        /* For each type param, look at the corresponding argument's type */
                        if (i < node->data.call.args.count) {
                            AstNode *arg = node->data.call.args.items[i];
                            /* Try to infer the type from the argument's resolved declaration */
                            if (arg->type == NODE_IDENT && arg->data.ident.resolved) {
                                AstNode *decl = arg->data.ident.resolved;
                                AstNode *type_node = NULL;
                                if (decl->type == NODE_LET) type_node = decl->data.let_decl.type;
                                else if (decl->type == NODE_PARAM) type_node = decl->data.param.type;
                                if (type_node) {
                                    node_list_append(&concrete_types, type_node);
                                    continue;
                                }
                            }
                            /* Fallback: use int64_t for numeric literals */
                            if (arg->type == NODE_LITERAL_INT) {
                                node_list_append(&concrete_types, node_type_prim(sa->arena, arg->loc, PRIM_I64));
                                continue;
                            }
                            if (arg->type == NODE_LITERAL_FLOAT) {
                                node_list_append(&concrete_types, node_type_prim(sa->arena, arg->loc, PRIM_F64));
                                continue;
                            }
                            if (arg->type == NODE_LITERAL_STRING) {
                                node_list_append(&concrete_types, node_type_prim(sa->arena, arg->loc, PRIM_STRING));
                                continue;
                            }
                            if (arg->type == NODE_LITERAL_BOOL) {
                                node_list_append(&concrete_types, node_type_prim(sa->arena, arg->loc, PRIM_BOOL));
                                continue;
                            }
                        }
                        /* Default fallback: int64_t */
                        node_list_append(&concrete_types, node_type_prim(sa->arena, NO_LOCATION, PRIM_I64));
                    }
                    /* Create monomorphized copy and redirect the call */
                    AstNode *mono = monomorphize_func(sa->arena, sa->program, func, &concrete_types);
                    node->data.call.callee->data.ident.resolved = mono;
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
        case NODE_ASM_BLOCK:
            break;

        case NODE_LAMBDA: {
            /* Visit lambda body to resolve identifiers and detect captures */
            scope_enter(sa, node);
            /* Declare lambda parameters in lambda scope */
            for (int i = 0; i < node->data.lambda.params.count; i++) {
                AstNode *param = node->data.lambda.params.items[i];
                if (param->data.param.name && param->data.param.name->type == NODE_IDENT) {
                    const char *pname = arena_strndup(sa->arena,
                        param->data.param.name->data.ident.name.data,
                        param->data.param.name->data.ident.name.len);
                    scope_declare(sa, pname, param);
                }
            }
            /* Visit return type if present */
            if (node->data.lambda.return_type) {
                semantic_visit_node(sa, node->data.lambda.return_type);
            }
            /* Visit body */
            if (node->data.lambda.body) {
                semantic_visit_expr(sa, node->data.lambda.body);
            }
            scope_exit(sa);
            break;
        }

        case NODE_ARRAY_LIT:
            /* Visit array literal elements for name resolution */
            for (int i = 0; i < node->data.array_lit.elements.count; i++) {
                semantic_visit_expr(sa, node->data.array_lit.elements.items[i]);
            }
            break;

        default:
            break;
    }
}

void semantic_analyze(SemanticAnalyzer *sa, AstNode *program) {
    sa->program = program;
    /* Register built-in functions */
    AstNode *print_decl = (AstNode *)arena_alloc(sa->arena, sizeof(AstNode));
    memset(print_decl, 0, sizeof(AstNode));
    print_decl->type = NODE_FUNC_DECL;
    scope_declare(sa, "print", print_decl);

    AstNode *exit_decl = (AstNode *)arena_alloc(sa->arena, sizeof(AstNode));
    memset(exit_decl, 0, sizeof(AstNode));
    exit_decl->type = NODE_FUNC_DECL;
    scope_declare(sa, "exit", exit_decl);

    semantic_visit_node(sa, program);
}