#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Create C codegen
 * ────────────────────────────────────────────── */
CCodegen *c_create(Arena *arena, Target target, int opt_level) {
    CCodegen *cg = (CCodegen *)arena_alloc(arena, sizeof(CCodegen));
    memset(cg, 0, sizeof(*cg));
    cg->arena = arena;
    cg->target = target;
    cg->opt_level = opt_level;
    cg->indent = 0;
    cg->str_counter = 0;
    cg->label_counter = 0;
    return cg;
}

/* ──────────────────────────────────────────────
 * Indentation helpers
 * ────────────────────────────────────────────── */
void c_indent(CCodegen *cg) {
    for (int i = 0; i < cg->indent; i++)
        fputs("    ", cg->out);
}

void c_newline(CCodegen *cg) {
    fputc('\n', cg->out);
}

/* ──────────────────────────────────────────────
 * Forward declarations for module functions
 * ────────────────────────────────────────────── */
void c_emit_type(CCodegen *cg, AstNode *type_node);
void c_emit_prim_type(CCodegen *cg, PrimType prim);
const char *c_type_name(AstNode *type_node);
const char *c_prim_type_name(PrimType prim);
void c_emit_expr(CCodegen *cg, AstNode *node);
void c_emit_stmt(CCodegen *cg, AstNode *node);
void c_emit_block(CCodegen *cg, AstNode *node);
void c_emit_func_decl(CCodegen *cg, AstNode *node);
void c_emit_func_prototype(CCodegen *cg, AstNode *node);
void c_emit_runtime(CCodegen *cg);
void c_emit_target_preamble(CCodegen *cg);
void c_emit_target_postamble(CCodegen *cg);
int c_compile(CCodegen *cg, const char *c_path, const char *output_path);

/* ──────────────────────────────────────────────
 * Generate C code for a complete program
 * ────────────────────────────────────────────── */

/* Recursively walk an AST node and collect all lambda expressions.
 * Each lambda gets a unique ID and is registered in cg->lambda_decls. */
static void collect_lambdas(CCodegen *cg, AstNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_LAMBDA: {
            int lambda_id = cg->lambda_counter++;
            AstNode *lambda_func = node_create(cg->arena, NODE_FUNC_DECL, node->loc);
            char lname[64];
            int lname_len = snprintf(lname, sizeof(lname), "__lambda_%d", lambda_id);
            /* Arena-allocate the name so it persists */
            char *name_copy = (char *)arena_alloc(cg->arena, (size_t)lname_len + 1);
            memcpy(name_copy, lname, (size_t)lname_len + 1);
            lambda_func->data.func.name = node_ident(cg->arena, node->loc,
                (StringView){name_copy, (size_t)lname_len});
            lambda_func->data.func.params = node->data.lambda.params;
            lambda_func->data.func.return_type = node->data.lambda.return_type;
            /* If no return type specified, infer from body: block body = void, expr body = int64_t */
            if (!lambda_func->data.func.return_type) {
                if (node->data.lambda.body && node->data.lambda.body->type != NODE_BLOCK) {
                    lambda_func->data.func.return_type = node_type_prim(cg->arena, node->loc, PRIM_I64);
                }
            }
            lambda_func->data.func.body = node->data.lambda.body;
            lambda_func->data.func.is_static = true;
            node_list_append(&cg->lambda_decls, lambda_func);
            /* Store the lambda ID on the node for later use in expression emission */
            node->data.lambda.captures = (AstNode*)(intptr_t)lambda_id;
            /* Recurse into body for nested lambdas */
            collect_lambdas(cg, node->data.lambda.body);
            break;
        }
        case NODE_BLOCK:
            for (int i = 0; i < node->data.list.count; i++)
                collect_lambdas(cg, node->data.list.items[i]);
            break;
        case NODE_FUNC_DECL:
        case NODE_PROPERTY:
            collect_lambdas(cg, node->data.func.body);
            break;
        case NODE_LET:
            collect_lambdas(cg, node->data.let_decl.value);
            break;
        case NODE_RETURN:
            collect_lambdas(cg, node->data.return_node.value);
            break;
        case NODE_IF:
            collect_lambdas(cg, node->data.if_node.condition);
            collect_lambdas(cg, node->data.if_node.then_block);
            collect_lambdas(cg, node->data.if_node.elif_chain);
            collect_lambdas(cg, node->data.if_node.else_block);
            break;
        case NODE_WHILE:
            collect_lambdas(cg, node->data.while_node.condition);
            collect_lambdas(cg, node->data.while_node.body);
            break;
        case NODE_FOR:
            collect_lambdas(cg, node->data.for_node.iterable);
            collect_lambdas(cg, node->data.for_node.body);
            break;
        case NODE_BINARY_OP:
            collect_lambdas(cg, node->data.binary.left);
            collect_lambdas(cg, node->data.binary.right);
            break;
        case NODE_UNARY_OP:
            collect_lambdas(cg, node->data.unary.operand);
            break;
        case NODE_CALL:
            collect_lambdas(cg, node->data.call.callee);
            for (int i = 0; i < node->data.call.args.count; i++)
                collect_lambdas(cg, node->data.call.args.items[i]);
            break;
        case NODE_EXPR_STMT:
            collect_lambdas(cg, node->data.call.callee);
            break;
        default:
            break;
    }
}

bool c_generate(CCodegen *cg, AstNode *program, FILE *out) {
    if (!program || program->type != NODE_PROGRAM) {
        fprintf(stderr, "C: expected NODE_PROGRAM\n");
        return false;
    }

    cg->out = out;
    cg->program = program;

    /* Pass 0: Collect all lambda expressions from the AST (pre-pass before emission) */
    cg->lambda_decls = (AstNodeList){0};
    cg->lambda_counter = 0;
    /* Walk the entire AST to find and register all lambda expressions */
    for (int i = 0; i < program->data.list.count; i++) {
        collect_lambdas(cg, program->data.list.items[i]);
    }

    /* Emit target preamble (includes, main wrapper) */
    c_emit_target_preamble(cg);

    /* Emit runtime helpers */
    c_emit_runtime(cg);

    /* Pass 0: Emit type declarations (struct, enum, class, type alias, trait, impl) before function prototypes */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        switch (decl->type) {
            case NODE_STRUCT_DECL:
            case NODE_CLASS_DECL:
            case NODE_ENUM_DECL:
            case NODE_TYPE_ALIAS:
            case NODE_TRAIT_DECL:
            case NODE_IMPL_BLOCK:
                c_emit_stmt(cg, decl);
                break;
            default:
                break;
        }
    }
    /* Emit default _drop stubs for classes (after type declarations, before function prototypes) */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        if (decl->type == NODE_CLASS_DECL && decl->data.struct_decl.name &&
            decl->data.struct_decl.name->type == NODE_IDENT) {
            StringView cn = decl->data.struct_decl.name->data.ident.name;
            c_indent(cg);
            fprintf(cg->out, "static void %.*s_drop(void *self) { (void)self; }\n", (int)cn.len, cn.data);
        }
    }

    /* Pass 1: Emit function prototypes (forward declarations) */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        if (decl->type == NODE_FUNC_DECL && decl->data.func.type_params.count == 0) {
            c_emit_func_prototype(cg, decl);
        }
        /* Also emit prototypes for struct/class methods */
        if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_CLASS_DECL) {
            for (int mi = 0; mi < decl->data.struct_decl.methods.count; mi++) {
                AstNode *method = decl->data.struct_decl.methods.items[mi];
                /* Skip impl block methods — they're emitted inline in the impl block handler */
                if (method->type == NODE_FUNC_DECL && method->data.func.is_impl_method) continue;
                c_emit_func_prototype(cg, method);
            }
        }
        /* Impl block method prototypes are emitted inline in the impl block handler */
    }
    /* Emit prototypes for lambda functions collected during expression emission */
    for (int i = 0; i < cg->lambda_decls.count; i++) {
        c_emit_func_prototype(cg, cg->lambda_decls.items[i]);
    }

    /* Pass 2a: Emit global variable declarations (before function bodies) */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        switch (decl->type) {
            case NODE_LET:
                if (decl->data.let_decl.is_global) {
                    c_emit_stmt(cg, decl);
                }
                break;
            case NODE_CONST_DECL:
                c_emit_stmt(cg, decl);
                break;
            default:
                break;
        }
    }

    /* Pass 2b: Emit function bodies */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        if (decl->type == NODE_FUNC_DECL) {
            c_emit_func_decl(cg, decl);
        }
        /* Also emit struct/class method bodies */
        if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_CLASS_DECL) {
            for (int mi = 0; mi < decl->data.struct_decl.methods.count; mi++) {
                AstNode *method = decl->data.struct_decl.methods.items[mi];
                /* Skip impl block methods — they're emitted inline in the impl block handler */
                if (method->type == NODE_FUNC_DECL && method->data.func.is_impl_method) continue;
                c_emit_func_decl(cg, method);
            }
        }
    }

    /* Pass 2c: Emit lambda function bodies collected during expression emission */
    for (int i = 0; i < cg->lambda_decls.count; i++) {
        c_emit_func_decl(cg, cg->lambda_decls.items[i]);
    }

    /* Check if we need an auto-generated test dispatcher */
    bool has_main = false;
    int test_func_count = 0;
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *node = program->data.list.items[i];
        if (node->type == NODE_FUNC_DECL) {
            StringView fname = node->data.func.name->data.ident.name;
            if (fname.len == 4 && memcmp(fname.data, "main", 4) == 0) {
                has_main = true;
            }
            if (node->data.func.has_test) {
                test_func_count++;
            }
        }
    }

    if (test_func_count > 0 && !has_main) {
        /* Auto-generate test dispatcher: run all @test functions, return __test_failures */
        fputs("int main(int argc, char **argv) {\n", cg->out);
        fputs("    (void)argc; (void)argv;\n", cg->out);
        for (int i = 0; i < program->data.list.count; i++) {
            AstNode *node = program->data.list.items[i];
            if (node->type != NODE_FUNC_DECL || !node->data.func.has_test) continue;
            StringView fname = node->data.func.name->data.ident.name;
            fprintf(cg->out, "    %.*s();\n", (int)fname.len, fname.data);
        }
        fputs("    return (int)__test_failures;\n", cg->out);
        fputs("}\n", cg->out);
    }

    /* Emit target postamble */
    c_emit_target_postamble(cg);

    return true;
}
