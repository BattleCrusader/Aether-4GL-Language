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
bool c_generate(CCodegen *cg, AstNode *program, FILE *out) {
    if (!program || program->type != NODE_PROGRAM) {
        fprintf(stderr, "C: expected NODE_PROGRAM\n");
        return false;
    }

    cg->out = out;
    cg->program = program;

    /* Emit target preamble (includes, main wrapper) */
    c_emit_target_preamble(cg);

    /* Emit runtime helpers */
    c_emit_runtime(cg);

    /* Pass 0: Emit type declarations (struct, enum, class, type alias) before function prototypes */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        switch (decl->type) {
            case NODE_STRUCT_DECL:
            case NODE_CLASS_DECL:
            case NODE_ENUM_DECL:
            case NODE_TYPE_ALIAS:
                c_emit_stmt(cg, decl);
                break;
            default:
                break;
        }
    }

    /* Pass 1: Emit function prototypes (forward declarations) */
    for (int i = 0; i < program->data.list.count; i++) {
        AstNode *decl = program->data.list.items[i];
        if (decl->type == NODE_FUNC_DECL) {
            c_emit_func_prototype(cg, decl);
        }
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
