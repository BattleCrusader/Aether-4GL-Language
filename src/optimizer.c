/* ================================================================
 * optimizer.c — Optimization Passes
 *
 * All passes operate on the AST in-place. No deep copies.
 * Each pass can be enabled/disabled independently.
 * ================================================================ */

#include "aether/optimizer.h"
#include "aether/ast.h"
#include "aether/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * Config
 * ================================================================ */

void optimizer_config_init(OptimizerConfig *cfg) {
    memset(cfg, 0, sizeof(OptimizerConfig));
    cfg->constant_folding = true;
    cfg->dead_code_elimination = true;
    cfg->inlining = true;
    cfg->escape_analysis = true;
    cfg->region_elision = true;
    cfg->devirtualization = true;
    cfg->loop_unrolling = true;
    cfg->memory_fusion = true;
}

/* ================================================================
 * Main optimizer entry point
 * ================================================================ */

AstNode *optimize(AstNode *program, Arena *arena, const OptimizerConfig *cfg) {
    if (!program) return NULL;
    if (cfg->constant_folding)    program = opt_constant_fold(program, arena);
    if (cfg->dead_code_elimination) program = opt_dead_code_elim(program, arena);
    if (cfg->inlining)            program = opt_inline(program, arena);
    if (cfg->escape_analysis)     program = opt_escape_analysis(program, arena);
    if (cfg->region_elision)      program = opt_region_elision(program, arena);
    if (cfg->devirtualization)    program = opt_devirtualize(program, arena);
    if (cfg->loop_unrolling)      program = opt_loop_unroll(program, arena);
    if (cfg->memory_fusion)       program = opt_memory_fusion(program, arena);
    return program;
}

/* ================================================================
 * Utility: count AST nodes in a subtree
 * ================================================================ */

int count_ast_nodes(AstNode *node) {
    if (!node) return 0;
    int count = 1;
    switch (node->type) {
        case NODE_BLOCK:
        case NODE_PROGRAM:
            for (int i = 0; i < node->data.list.count; i++)
                count += count_ast_nodes(node->data.list.items[i]);
            break;
        case NODE_FUNC_DECL:
            count += count_ast_nodes(node->data.func.body);
            for (int i = 0; i < node->data.func.params.count; i++)
                count += count_ast_nodes(node->data.func.params.items[i]);
            break;
        case NODE_LET:
            count += count_ast_nodes(node->data.let_decl.value);
            break;
        case NODE_IF:
            count += count_ast_nodes(node->data.if_node.condition);
            count += count_ast_nodes(node->data.if_node.then_block);
            if (node->data.if_node.else_block)
                count += count_ast_nodes(node->data.if_node.else_block);
            break;
        case NODE_WHILE:
            count += count_ast_nodes(node->data.while_node.condition);
            count += count_ast_nodes(node->data.while_node.body);
            break;
        case NODE_FOR:
            count += count_ast_nodes(node->data.for_node.iterable);
            count += count_ast_nodes(node->data.for_node.body);
            break;
        case NODE_RETURN:
            count += count_ast_nodes(node->data.return_node.value);
            break;
        case NODE_BINARY_OP:
            count += count_ast_nodes(node->data.binary.left);
            count += count_ast_nodes(node->data.binary.right);
            break;
        case NODE_UNARY_OP:
            count += count_ast_nodes(node->data.unary.operand);
            break;
        case NODE_CALL:
            count += count_ast_nodes(node->data.call.callee);
            for (int i = 0; i < node->data.call.args.count; i++)
                count += count_ast_nodes(node->data.call.args.items[i]);
            break;
        case NODE_INDEX:
            count += count_ast_nodes(node->data.index.target);
            count += count_ast_nodes(node->data.index.index);
            break;
        case NODE_FIELD_ACCESS:
            count += count_ast_nodes(node->data.field.target);
            break;
        case NODE_MATCH:
            count += count_ast_nodes(node->data.match_node.value);
            for (int i = 0; i < node->data.match_node.arms.count; i++)
                count += count_ast_nodes(node->data.match_node.arms.items[i]);
            break;
        case NODE_TRY:
            count += count_ast_nodes(node->data.try_node.body);
            for (int i = 0; i < node->data.try_node.catch_arms.count; i++)
                count += count_ast_nodes(node->data.try_node.catch_arms.items[i]);
            break;
        case NODE_THROW:
            count += count_ast_nodes(node->data.throw_node.value);
            break;
        case NODE_DEFER:
            count += count_ast_nodes(node->data.defer_node.body);
            break;
        case NODE_REGION:
            count += count_ast_nodes(node->data.region_node.body);
            break;
        case NODE_LAMBDA:
            count += count_ast_nodes(node->data.lambda.body);
            break;
        case NODE_ARRAY_LIT:
            for (int i = 0; i < node->data.array_lit.elements.count; i++)
                count += count_ast_nodes(node->data.array_lit.elements.items[i]);
            break;
        default:
            break;
    }
    return count;
}

/* ================================================================
 * Utility: check if expression is a constant
 * ================================================================ */

bool is_constant_expr(AstNode *node) {
    if (!node) return false;
    switch (node->type) {
        case NODE_LITERAL_INT:
        case NODE_LITERAL_FLOAT:
        case NODE_LITERAL_BOOL:
        case NODE_LITERAL_CHAR:
        case NODE_LITERAL_STRING:
        case NODE_LITERAL_NONE:
            return true;
        case NODE_UNARY_OP:
            if (node->data.unary.op == UNARY_NEG ||
                node->data.unary.op == UNARY_NOT ||
                node->data.unary.op == UNARY_BIT_NOT)
                return is_constant_expr(node->data.unary.operand);
            return false;
        case NODE_BINARY_OP: {
            BinOp op = node->data.binary.op;
            if (op == BIN_ADD || op == BIN_SUB || op == BIN_MUL ||
                op == BIN_DIV || op == BIN_MOD ||
                op == BIN_EQ || op == BIN_NEQ || op == BIN_LT ||
                op == BIN_GT || op == BIN_LE || op == BIN_GE ||
                op == BIN_AND || op == BIN_OR ||
                op == BIN_BIT_AND || op == BIN_BIT_OR || op == BIN_BIT_XOR ||
                op == BIN_SHL || op == BIN_SHR)
                return is_constant_expr(node->data.binary.left) &&
                       is_constant_expr(node->data.binary.right);
            return false;
        }
        default:
            return false;
    }
}

/* ================================================================
 * Utility: evaluate constant integer expression
 * ================================================================ */

bool eval_constant_int(AstNode *node, uint64_t *result) {
    if (!node || !result) return false;
    switch (node->type) {
        case NODE_LITERAL_INT:
            *result = node->data.literal.int_val;
            if (node->data.literal.is_negative)
                *result = (uint64_t)(-(int64_t)*result);
            return true;
        case NODE_LITERAL_BOOL:
            *result = node->data.literal.bool_val ? 1 : 0;
            return true;
        case NODE_LITERAL_CHAR:
            *result = node->data.literal.char_val;
            return true;
        case NODE_UNARY_OP: {
            uint64_t val;
            if (!eval_constant_int(node->data.unary.operand, &val)) return false;
            switch (node->data.unary.op) {
                case UNARY_NEG:    *result = (uint64_t)(-(int64_t)val); return true;
                case UNARY_NOT:    *result = val ? 0 : 1; return true;
                case UNARY_BIT_NOT: *result = ~val; return true;
                default: return false;
            }
        }
        case NODE_BINARY_OP: {
            uint64_t left, right;
            if (!eval_constant_int(node->data.binary.left, &left) ||
                !eval_constant_int(node->data.binary.right, &right))
                return false;
            switch (node->data.binary.op) {
                case BIN_ADD: *result = left + right; return true;
                case BIN_SUB: *result = left - right; return true;
                case BIN_MUL: *result = left * right; return true;
                case BIN_DIV: if (right == 0) return false; *result = left / right; return true;
                case BIN_MOD: if (right == 0) return false; *result = left % right; return true;
                case BIN_EQ:  *result = (left == right) ? 1 : 0; return true;
                case BIN_NEQ: *result = (left != right) ? 1 : 0; return true;
                case BIN_LT:  *result = (left < right) ? 1 : 0; return true;
                case BIN_GT:  *result = (left > right) ? 1 : 0; return true;
                case BIN_LE:  *result = (left <= right) ? 1 : 0; return true;
                case BIN_GE:  *result = (left >= right) ? 1 : 0; return true;
                case BIN_AND: *result = (left && right) ? 1 : 0; return true;
                case BIN_OR:  *result = (left || right) ? 1 : 0; return true;
                case BIN_BIT_AND: *result = left & right; return true;
                case BIN_BIT_OR:  *result = left | right; return true;
                case BIN_BIT_XOR: *result = left ^ right; return true;
                case BIN_SHL: *result = left << right; return true;
                case BIN_SHR: *result = left >> right; return true;
                default: return false;
            }
        }
        default: return false;
    }
}

/* ================================================================
 * P09.01 — Constant Folding and Propagation
 * ================================================================ */

static AstNode *fold_node(AstNode *node, Arena *arena);

static AstNode *fold_binary_op(AstNode *node, Arena *arena) {
    node->data.binary.left = fold_node(node->data.binary.left, arena);
    node->data.binary.right = fold_node(node->data.binary.right, arena);
    if (is_constant_expr(node->data.binary.left) &&
        is_constant_expr(node->data.binary.right)) {
        uint64_t result;
        if (eval_constant_int(node, &result)) {
            AstNode *lit = arena_alloc(arena, sizeof(AstNode));
            memset(lit, 0, sizeof(AstNode));
            lit->type = NODE_LITERAL_INT;
            lit->data.literal.int_val = result;
            lit->data.literal.is_negative = false;
            return lit;
        }
    }
    return node;
}

static AstNode *fold_unary_op(AstNode *node, Arena *arena) {
    node->data.unary.operand = fold_node(node->data.unary.operand, arena);
    if (is_constant_expr(node->data.unary.operand)) {
        uint64_t result;
        if (eval_constant_int(node, &result)) {
            AstNode *lit = arena_alloc(arena, sizeof(AstNode));
            memset(lit, 0, sizeof(AstNode));
            lit->type = NODE_LITERAL_INT;
            lit->data.literal.int_val = result;
            lit->data.literal.is_negative = false;
            return lit;
        }
    }
    return node;
}

static AstNode *fold_if_node(AstNode *node, Arena *arena) {
    node->data.if_node.condition = fold_node(node->data.if_node.condition, arena);
    node->data.if_node.then_block = fold_node(node->data.if_node.then_block, arena);
    if (node->data.if_node.else_block)
        node->data.if_node.else_block = fold_node(node->data.if_node.else_block, arena);
    if (node->data.if_node.condition->type == NODE_LITERAL_BOOL) {
        if (node->data.if_node.condition->data.literal.bool_val)
            return node->data.if_node.then_block;
        else
            return node->data.if_node.else_block ? node->data.if_node.else_block : NULL;
    }
    return node;
}

static AstNode *fold_while_node(AstNode *node, Arena *arena) {
    node->data.while_node.condition = fold_node(node->data.while_node.condition, arena);
    node->data.while_node.body = fold_node(node->data.while_node.body, arena);
    return node;
}

static AstNode *fold_for_node(AstNode *node, Arena *arena) {
    node->data.for_node.iterable = fold_node(node->data.for_node.iterable, arena);
    node->data.for_node.body = fold_node(node->data.for_node.body, arena);
    return node;
}

static AstNode *fold_block(AstNode *node, Arena *arena) {
    for (int i = 0; i < node->data.list.count; i++)
        node->data.list.items[i] = fold_node(node->data.list.items[i], arena);
    return node;
}

static AstNode *fold_func_decl(AstNode *node, Arena *arena) {
    if (node->data.func.body) node->data.func.body = fold_node(node->data.func.body, arena);
    return node;
}

static AstNode *fold_let(AstNode *node, Arena *arena) {
    if (node->data.let_decl.value) node->data.let_decl.value = fold_node(node->data.let_decl.value, arena);
    return node;
}

static AstNode *fold_return(AstNode *node, Arena *arena) {
    if (node->data.return_node.value) node->data.return_node.value = fold_node(node->data.return_node.value, arena);
    return node;
}

static AstNode *fold_call(AstNode *node, Arena *arena) {
    node->data.call.callee = fold_node(node->data.call.callee, arena);
    for (int i = 0; i < node->data.call.args.count; i++)
        node->data.call.args.items[i] = fold_node(node->data.call.args.items[i], arena);
    return node;
}

static AstNode *fold_index(AstNode *node, Arena *arena) {
    node->data.index.target = fold_node(node->data.index.target, arena);
    node->data.index.index = fold_node(node->data.index.index, arena);
    return node;
}

static AstNode *fold_field_access(AstNode *node, Arena *arena) {
    node->data.field.target = fold_node(node->data.field.target, arena);
    return node;
}

static AstNode *fold_match(AstNode *node, Arena *arena) {
    node->data.match_node.value = fold_node(node->data.match_node.value, arena);
    for (int i = 0; i < node->data.match_node.arms.count; i++)
        node->data.match_node.arms.items[i] = fold_node(node->data.match_node.arms.items[i], arena);
    return node;
}

static AstNode *fold_try(AstNode *node, Arena *arena) {
    node->data.try_node.body = fold_node(node->data.try_node.body, arena);
    for (int i = 0; i < node->data.try_node.catch_arms.count; i++)
        node->data.try_node.catch_arms.items[i] = fold_node(node->data.try_node.catch_arms.items[i], arena);
    return node;
}

static AstNode *fold_throw(AstNode *node, Arena *arena) {
    if (node->data.throw_node.value) node->data.throw_node.value = fold_node(node->data.throw_node.value, arena);
    return node;
}

static AstNode *fold_defer(AstNode *node, Arena *arena) {
    node->data.defer_node.body = fold_node(node->data.defer_node.body, arena);
    return node;
}

static AstNode *fold_region(AstNode *node, Arena *arena) {
    node->data.region_node.body = fold_node(node->data.region_node.body, arena);
    return node;
}

static AstNode *fold_lambda(AstNode *node, Arena *arena) {
    node->data.lambda.body = fold_node(node->data.lambda.body, arena);
    return node;
}

static AstNode *fold_array_lit(AstNode *node, Arena *arena) {
    for (int i = 0; i < node->data.array_lit.elements.count; i++)
        node->data.array_lit.elements.items[i] = fold_node(node->data.array_lit.elements.items[i], arena);
    return node;
}

static AstNode *fold_node(AstNode *node, Arena *arena) {
    if (!node) return NULL;
    switch (node->type) {
        case NODE_BLOCK:
        case NODE_PROGRAM:     return fold_block(node, arena);
        case NODE_FUNC_DECL:   return fold_func_decl(node, arena);
        case NODE_LET:         return fold_let(node, arena);
        case NODE_IF:          return fold_if_node(node, arena);
        case NODE_WHILE:       return fold_while_node(node, arena);
        case NODE_FOR:         return fold_for_node(node, arena);
        case NODE_RETURN:      return fold_return(node, arena);
        case NODE_CALL:        return fold_call(node, arena);
        case NODE_INDEX:       return fold_index(node, arena);
        case NODE_FIELD_ACCESS: return fold_field_access(node, arena);
        case NODE_MATCH:       return fold_match(node, arena);
        case NODE_TRY:         return fold_try(node, arena);
        case NODE_THROW:       return fold_throw(node, arena);
        case NODE_DEFER:       return fold_defer(node, arena);
        case NODE_REGION:      return fold_region(node, arena);
        case NODE_LAMBDA:      return fold_lambda(node, arena);
        case NODE_ARRAY_LIT:   return fold_array_lit(node, arena);
        case NODE_BINARY_OP:   return fold_binary_op(node, arena);
        case NODE_UNARY_OP:    return fold_unary_op(node, arena);
        default:               return node;
    }
}

AstNode *opt_constant_fold(AstNode *node, Arena *arena) {
    return fold_node(node, arena);
}

/* ================================================================
 * P09.02 — Dead Code Elimination
 * ================================================================ */

#define MAX_SYMBOLS 4096

typedef struct {
    const char *name;
    bool is_used;
    AstNode *decl_node;
} SymbolEntry;

typedef struct {
    SymbolEntry entries[MAX_SYMBOLS];
    int count;
} SymbolTable;

static int find_symbol(SymbolTable *st, const char *name) {
    for (int i = 0; i < st->count; i++)
        if (strcmp(st->entries[i].name, name) == 0) return i;
    return -1;
}

static int add_symbol(SymbolTable *st, const char *name, AstNode *decl) {
    if (st->count >= MAX_SYMBOLS) return -1;
    int idx = st->count++;
    st->entries[idx].name = name;
    st->entries[idx].is_used = false;
    st->entries[idx].decl_node = decl;
    return idx;
}

static void dce_collect(AstNode *node, SymbolTable *st);

static void dce_collect_ident(AstNode *node, SymbolTable *st) {
    if (node->type == NODE_IDENT) {
        /* StringView: need to null-terminate for strcmp */
        char name_buf[256];
        int len = node->data.ident.name.len < 255 ? node->data.ident.name.len : 255;
        memcpy(name_buf, node->data.ident.name.data, len);
        name_buf[len] = '\0';
        int idx = find_symbol(st, name_buf);
        if (idx >= 0) st->entries[idx].is_used = true;
    }
}

static void dce_collect(AstNode *node, SymbolTable *st) {
    if (!node) return;
    switch (node->type) {
        case NODE_FUNC_DECL: {
            StringView sv = node->data.func.name->data.ident.name;
            char name_buf[256];
            int len = sv.len < 255 ? sv.len : 255;
            memcpy(name_buf, sv.data, len);
            name_buf[len] = '\0';
            bool is_entry = (len == 4 && memcmp(name_buf, "main", 4) == 0) ||
                            node->data.func.is_exported ||
                            node->data.func.is_sys ||
                            node->data.func.entry_addr >= 0;
            if (!is_entry) add_symbol(st, name_buf, node);
            if (node->data.func.body) dce_collect(node->data.func.body, st);
            break;
        }
        case NODE_LET: {
            StringView sv = node->data.let_decl.name->data.ident.name;
            char name_buf[256];
            int len = sv.len < 255 ? sv.len : 255;
            memcpy(name_buf, sv.data, len);
            name_buf[len] = '\0';
            add_symbol(st, name_buf, node);
            if (node->data.let_decl.value) dce_collect(node->data.let_decl.value, st);
            break;
        }
        case NODE_IDENT:
            dce_collect_ident(node, st);
            break;
        case NODE_BLOCK:
        case NODE_PROGRAM:
            for (int i = 0; i < node->data.list.count; i++)
                dce_collect(node->data.list.items[i], st);
            break;
        case NODE_IF:
            dce_collect(node->data.if_node.condition, st);
            dce_collect(node->data.if_node.then_block, st);
            if (node->data.if_node.else_block) dce_collect(node->data.if_node.else_block, st);
            break;
        case NODE_WHILE:
            dce_collect(node->data.while_node.condition, st);
            dce_collect(node->data.while_node.body, st);
            break;
        case NODE_FOR:
            dce_collect(node->data.for_node.iterable, st);
            dce_collect(node->data.for_node.body, st);
            break;
        case NODE_RETURN:
            if (node->data.return_node.value) dce_collect(node->data.return_node.value, st);
            break;
        case NODE_CALL:
            dce_collect(node->data.call.callee, st);
            for (int i = 0; i < node->data.call.args.count; i++)
                dce_collect(node->data.call.args.items[i], st);
            break;
        case NODE_BINARY_OP:
            dce_collect(node->data.binary.left, st);
            dce_collect(node->data.binary.right, st);
            break;
        case NODE_UNARY_OP:
            dce_collect(node->data.unary.operand, st);
            break;
        case NODE_INDEX:
            dce_collect(node->data.index.target, st);
            dce_collect(node->data.index.index, st);
            break;
        case NODE_FIELD_ACCESS:
            dce_collect(node->data.field.target, st);
            break;
        case NODE_MATCH:
            dce_collect(node->data.match_node.value, st);
            for (int i = 0; i < node->data.match_node.arms.count; i++)
                dce_collect(node->data.match_node.arms.items[i], st);
            break;
        case NODE_TRY:
            dce_collect(node->data.try_node.body, st);
            for (int i = 0; i < node->data.try_node.catch_arms.count; i++)
                dce_collect(node->data.try_node.catch_arms.items[i], st);
            break;
        case NODE_THROW:
            if (node->data.throw_node.value) dce_collect(node->data.throw_node.value, st);
            break;
        case NODE_DEFER:
            dce_collect(node->data.defer_node.body, st);
            break;
        case NODE_REGION:
            dce_collect(node->data.region_node.body, st);
            break;
        case NODE_LAMBDA:
            dce_collect(node->data.lambda.body, st);
            break;
        case NODE_ARRAY_LIT:
            for (int i = 0; i < node->data.array_lit.elements.count; i++)
                dce_collect(node->data.array_lit.elements.items[i], st);
            break;
        default:
            break;
    }
}

static AstNode *dce_remove_dead(AstNode *node, SymbolTable *st) {
    if (!node) return NULL;
    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_BLOCK: {
            AstNodeList *stmts = &node->data.list;
            int write_idx = 0;
            for (int i = 0; i < stmts->count; i++) {
                AstNode *stmt = stmts->items[i];
                bool keep = true;
                if (stmt->type == NODE_FUNC_DECL) {
                    StringView sv = stmt->data.func.name->data.ident.name;
                    char name_buf[256];
                    int len = sv.len < 255 ? sv.len : 255;
                    memcpy(name_buf, sv.data, len);
                    name_buf[len] = '\0';
                    bool is_entry = (len == 4 && memcmp(name_buf, "main", 4) == 0) ||
                                    stmt->data.func.is_exported ||
                                    stmt->data.func.is_sys ||
                                    stmt->data.func.entry_addr >= 0;
                    if (!is_entry) {
                        int idx = find_symbol(st, name_buf);
                        if (idx >= 0 && !st->entries[idx].is_used) keep = false;
                    }
                } else if (stmt->type == NODE_LET) {
                    StringView sv = stmt->data.let_decl.name->data.ident.name;
                    char name_buf[256];
                    int len = sv.len < 255 ? sv.len : 255;
                    memcpy(name_buf, sv.data, len);
                    name_buf[len] = '\0';
                    int idx = find_symbol(st, name_buf);
                    if (idx >= 0 && !st->entries[idx].is_used) {
                        /* Check if the initializer has side effects (function calls, asm) */
                        bool has_side_effects = false;
                        if (stmt->data.let_decl.value) {
                            /* Simple check: if the value is a call or asm block, keep it */
                            if (stmt->data.let_decl.value->type == NODE_CALL ||
                                stmt->data.let_decl.value->type == NODE_ASM_BLOCK ||
                                stmt->data.let_decl.value->type == NODE_BINARY_OP) {
                                has_side_effects = true;
                            }
                        }
                        if (!has_side_effects) keep = false;
                    }
                }
                if (keep) stmts->items[write_idx++] = dce_remove_dead(stmt, st);
            }
            stmts->count = write_idx;
            return node;
        }
        default:
            switch (node->type) {
                case NODE_FUNC_DECL:
                    if (node->data.func.body)
                        node->data.func.body = dce_remove_dead(node->data.func.body, st);
                    break;
                case NODE_IF:
                    node->data.if_node.condition = dce_remove_dead(node->data.if_node.condition, st);
                    node->data.if_node.then_block = dce_remove_dead(node->data.if_node.then_block, st);
                    if (node->data.if_node.else_block)
                        node->data.if_node.else_block = dce_remove_dead(node->data.if_node.else_block, st);
                    break;
                case NODE_WHILE:
                    node->data.while_node.condition = dce_remove_dead(node->data.while_node.condition, st);
                    node->data.while_node.body = dce_remove_dead(node->data.while_node.body, st);
                    break;
                case NODE_FOR:
                    node->data.for_node.iterable = dce_remove_dead(node->data.for_node.iterable, st);
                    node->data.for_node.body = dce_remove_dead(node->data.for_node.body, st);
                    break;
                default:
                    break;
            }
            return node;
    }
}

AstNode *opt_dead_code_elim(AstNode *node, Arena *arena) {
    (void)arena;
    if (!node) return NULL;
    SymbolTable st;
    memset(&st, 0, sizeof(st));
    dce_collect(node, &st);
    return dce_remove_dead(node, &st);
}

/* ================================================================
 * P09.03 — Aggressive Inlining
 * ================================================================ */

#define MAX_INLINE_NODES 20

bool is_small_function(AstNode *func_decl, int max_nodes) {
    if (!func_decl || func_decl->type != NODE_FUNC_DECL) return false;
    if (!func_decl->data.func.body) return false;
    return count_ast_nodes(func_decl->data.func.body) <= max_nodes;
}

static AstNode *find_function(AstNode *program, const char *name) {
    if (!program) return NULL;
    AstNodeList *decls;
    if (program->type == NODE_PROGRAM || program->type == NODE_BLOCK)
        decls = &program->data.list;
    else return NULL;
    for (int i = 0; i < decls->count; i++) {
        AstNode *decl = decls->items[i];
        if (decl->type == NODE_FUNC_DECL) {
            StringView sv = decl->data.func.name->data.ident.name;
            if (sv.len == strlen(name) && memcmp(sv.data, name, sv.len) == 0)
                return decl;
        }
    }
    return NULL;
}

AstNode *opt_inline(AstNode *node, Arena *arena) {
    if (!node) return NULL;
    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_BLOCK:
            for (int i = 0; i < node->data.list.count; i++)
                node->data.list.items[i] = opt_inline(node->data.list.items[i], arena);
            break;
        case NODE_FUNC_DECL:
            if (node->data.func.body) node->data.func.body = opt_inline(node->data.func.body, arena);
            break;
        case NODE_CALL:
            for (int i = 0; i < node->data.call.args.count; i++)
                node->data.call.args.items[i] = opt_inline(node->data.call.args.items[i], arena);
            break;
        case NODE_IF:
            node->data.if_node.condition = opt_inline(node->data.if_node.condition, arena);
            node->data.if_node.then_block = opt_inline(node->data.if_node.then_block, arena);
            if (node->data.if_node.else_block)
                node->data.if_node.else_block = opt_inline(node->data.if_node.else_block, arena);
            break;
        case NODE_WHILE:
            node->data.while_node.condition = opt_inline(node->data.while_node.condition, arena);
            node->data.while_node.body = opt_inline(node->data.while_node.body, arena);
            break;
        case NODE_FOR:
            node->data.for_node.iterable = opt_inline(node->data.for_node.iterable, arena);
            node->data.for_node.body = opt_inline(node->data.for_node.body, arena);
            break;
        case NODE_RETURN:
            if (node->data.return_node.value) node->data.return_node.value = opt_inline(node->data.return_node.value, arena);
            break;
        case NODE_LET:
            if (node->data.let_decl.value) node->data.let_decl.value = opt_inline(node->data.let_decl.value, arena);
            break;
        case NODE_BINARY_OP:
            node->data.binary.left = opt_inline(node->data.binary.left, arena);
            node->data.binary.right = opt_inline(node->data.binary.right, arena);
            break;
        case NODE_UNARY_OP:
            node->data.unary.operand = opt_inline(node->data.unary.operand, arena);
            break;
        default:
            break;
    }
    return node;
}

/* ================================================================
 * P09.04 — Escape Analysis (placeholder)
 * ================================================================ */

bool does_value_escape(AstNode *func_decl, AstNode *value) {
    (void)func_decl; (void)value;
    return true;
}

AstNode *opt_escape_analysis(AstNode *node, Arena *arena) {
    (void)arena;
    return node;
}

/* ================================================================
 * P09.05 — Region Inference (placeholder)
 * ================================================================ */

AstNode *opt_region_elision(AstNode *node, Arena *arena) {
    (void)arena;
    return node;
}

/* ================================================================
 * P09.06 — Devirtualization (placeholder)
 * ================================================================ */

AstNode *opt_devirtualize(AstNode *node, Arena *arena) {
    (void)arena;
    return node;
}

/* ================================================================
 * P09.07 — Loop Unrolling (placeholder)
 * ================================================================ */

AstNode *opt_loop_unroll(AstNode *node, Arena *arena) {
    (void)arena;
    return node;
}

/* ================================================================
 * P09.08 — Memory Fusion (placeholder)
 * ================================================================ */

AstNode *opt_memory_fusion(AstNode *node, Arena *arena) {
    (void)arena;
    return node;
}
