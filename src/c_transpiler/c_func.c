#include "aether/c_transpiler.h"
#include "aether/str.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ──────────────────────────────────────────────
 * Function codegen — emit C function declarations
 * ────────────────────────────────────────────── */

/* Emit a function prototype (forward declaration) */
void c_emit_func_prototype(CCodegen *cg, AstNode *node) {
    if (node->type != NODE_FUNC_DECL) return;

    StringView fname = node->data.func.name->data.ident.name;
    int param_count = node->data.func.params.count;
    AstNode *return_type_node = node->data.func.return_type;

    /* Skip main() — C handles it specially */
    int is_main = (fname.len == 4 && memcmp(fname.data, "main", 4) == 0);
    if (is_main) return;

    /* Emit return type */
    if (return_type_node) {
        c_emit_type(cg, return_type_node);
    } else {
        fputs("void", cg->out);
    }
    fputc(' ', cg->out);

    /* Emit function name */
    fprintf(cg->out, "%.*s(", (int)fname.len, fname.data);

    /* Emit parameters */
    for (int i = 0; i < param_count; i++) {
        if (i > 0) fputs(", ", cg->out);
        AstNode *param = node->data.func.params.items[i];
        AstNode *param_type = param->data.param.type;
        StringView pname = param->data.param.name->data.ident.name;

        if (param_type) {
            c_emit_type(cg, param_type);
        } else {
            fputs("uint64_t", cg->out);
        }
        fputc(' ', cg->out);
        fprintf(cg->out, "%.*s", (int)pname.len, pname.data);
    }
    fputs(");\n", cg->out);
}

/* Emit a full function definition */
void c_emit_func_decl(CCodegen *cg, AstNode *node) {
    if (node->type != NODE_FUNC_DECL) return;

    StringView fname = node->data.func.name->data.ident.name;
    int param_count = node->data.func.params.count;
    AstNode *return_type_node = node->data.func.return_type;
    AstNode *body = node->data.func.body;

    /* Emit return type */
    int is_main = (fname.len == 4 && memcmp(fname.data, "main", 4) == 0);
    if (is_main) {
        fputs("int", cg->out);
    } else if (return_type_node) {
        c_emit_type(cg, return_type_node);
    } else {
        fputs("void", cg->out);
    }
    fputc(' ', cg->out);

    /* Emit function name */
    fprintf(cg->out, "%.*s(", (int)fname.len, fname.data);

    /* Emit parameters */
    for (int i = 0; i < param_count; i++) {
        if (i > 0) fputs(", ", cg->out);
        AstNode *param = node->data.func.params.items[i];
        AstNode *param_type = param->data.param.type;
        StringView pname = param->data.param.name->data.ident.name;

        if (param_type) {
            c_emit_type(cg, param_type);
        } else {
            fputs("uint64_t", cg->out);
        }
        fputc(' ', cg->out);
        fprintf(cg->out, "%.*s", (int)pname.len, pname.data);
    }
    fputs(")", cg->out);

    /* Emit body */
    if (body) {
        fputs(" {\n", cg->out);
        cg->indent++;
        c_emit_block(cg, body);
        cg->indent--;
        c_indent(cg);
        fputs("}\n\n", cg->out);
    } else {
        fputs(";\n\n", cg->out);
    }
}
